#include "EditorPicking.h"

#include <algorithm>
#include <cmath>

#include "AssetDatabase.h"
#include "CollisionAsset.h"
#include "ModelLoader.h"
#include "Prefab.h"
#include "RadialTerrain.h"
#include "ResourceManager.h"
#include "SkeletalAnimation.h"
#include "TerrainLibrary.h"

namespace {
struct Ray { glm::dvec3 origin, direction; };

bool Finite(glm::dvec3 value) {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

bool LocalRay(const Ray& world, glm::vec3 position, glm::quat rotation, glm::vec3 scale, Ray& local) {
    if (!Finite(scale) || glm::any(glm::lessThanEqual(glm::abs(scale), glm::vec3(1e-8f))) ||
        !Finite(position) || !std::isfinite(glm::dot(rotation, rotation)) ||
        glm::dot(rotation, rotation) < 1e-12f) return false;
    const auto inverse = glm::conjugate(glm::normalize(glm::dquat(rotation)));
    local.origin = (inverse * (world.origin - glm::dvec3(position))) / glm::dvec3(scale);
    local.direction = (inverse * world.direction) / glm::dvec3(scale);
    // Do not normalize: the parameter remains a world-space distance after
    // inverse nonuniform scale, so nearest hits on different objects compare.
    return Finite(local.origin) && Finite(local.direction);
}

bool BoxHit(const Ray& ray, glm::dvec3 minimum, glm::dvec3 maximum, double& distance) {
    double near = -std::numeric_limits<double>::infinity();
    double far = std::numeric_limits<double>::infinity();
    for (int axis = 0; axis < 3; ++axis) {
        if (std::abs(ray.direction[axis]) < 1e-14) {
            if (ray.origin[axis] < minimum[axis] || ray.origin[axis] > maximum[axis]) return false;
            continue;
        }
        double a = (minimum[axis] - ray.origin[axis]) / ray.direction[axis];
        double b = (maximum[axis] - ray.origin[axis]) / ray.direction[axis];
        if (a > b) std::swap(a, b);
        near = std::max(near, a);
        far = std::min(far, b);
        if (near > far) return false;
    }
    distance = near >= 0 ? near : far;
    return distance >= 0 && std::isfinite(distance);
}

bool SphereHit(const Ray& ray, double radius, double& distance) {
    if (!(radius > 0) || !std::isfinite(radius)) return false;
    const double a = glm::dot(ray.direction, ray.direction);
    const double b = glm::dot(ray.origin, ray.direction);
    const double c = glm::dot(ray.origin, ray.origin) - radius * radius;
    const double discriminant = b * b - a * c;
    if (discriminant < 0 || !(a > 0)) return false;
    const double root = std::sqrt(discriminant);
    distance = (-b - root) / a;
    if (distance < 0) distance = (-b + root) / a;
    return distance >= 0 && std::isfinite(distance);
}

bool TriangleHit(const Ray& ray, glm::dvec3 a, glm::dvec3 b, glm::dvec3 c,
                 bool cullBack, double& distance) {
    const auto edge = b - a, other = c - a;
    const auto p = glm::cross(ray.direction, other);
    const double determinant = glm::dot(edge, p);
    const double epsilon = 1e-12 * std::sqrt(glm::dot(edge, edge) * glm::dot(p, p));
    if ((cullBack && determinant <= epsilon) || std::abs(determinant) <= epsilon) return false;
    const auto v = ray.origin - a;
    const double u = glm::dot(v, p) / determinant;
    if (u < -1e-10 || u > 1 + 1e-10) return false;
    const auto q = glm::cross(v, edge);
    const double w = glm::dot(ray.direction, q) / determinant;
    if (w < -1e-10 || u + w > 1 + 1e-10) return false;
    distance = glm::dot(other, q) / determinant;
    return distance >= 0 && std::isfinite(distance);
}

bool BackfaceCulling(const MeshData& mesh, std::size_t part, const SceneRenderComponent& render,
                     bool linear, const ResourceManager* resources) {
    std::shared_ptr<const MaterialDefinition> explicitMaterial;
    if (part < render.materials.size() && !render.materials[part].asset.empty() && resources) {
        explicitMaterial = resources->TryGetMaterialDefinition(render.materials[part].asset);
        // Renderer uses a single-sided Unlit material while an explicit
        // material is pending or failed; it does not fall back to Legacy.
        if (!explicitMaterial) return true;
    }
    const MaterialDefinition* material = explicitMaterial.get();
    // Imported materials are submitted only in the scene's linear mode.
    if (!material && linear && part < mesh.primitives.size() &&
        !(part < render.materials.size() && !render.materials[part].asset.empty())) {
        const auto slot = mesh.primitives[part].material;
        if (slot >= 0 && std::size_t(slot) < mesh.materials.size()) material = &mesh.materials[slot];
    }
    return material && material->model != MaterialModel::Legacy && !material->doubleSided;
}

bool MeshHit(const MeshData& mesh, const Ray& ray, const SceneRenderComponent& render,
             bool linear, const ResourceManager* resources, double& distance,
             const std::vector<bool>* mirroredParts = nullptr) {
    const auto count = mesh.indices.empty() ? mesh.vertices.size() : mesh.indices.size();
    bool hit = false;
    distance = std::numeric_limits<double>::infinity();
    const auto range = [&](std::size_t first, std::size_t size, bool cullBack, bool mirrored) {
        if (first > count) return;
        const auto end = first + std::min(size, count - first);
        for (auto n = first; n + 2 < end; n += 3) {
            const auto index = [&](std::size_t entry) { return mesh.indices.empty() ? entry : mesh.indices[entry]; };
            const auto a = index(n);
            auto b = index(n + 1), c = index(n + 2);
            if (a >= mesh.vertices.size() || b >= mesh.vertices.size() || c >= mesh.vertices.size()) continue;
            if (mirrored) std::swap(b, c);
            double candidate = 0;
            if (TriangleHit(ray, mesh.vertices[a].position, mesh.vertices[b].position,
                            mesh.vertices[c].position, cullBack, candidate) && candidate < distance) {
                distance = candidate;
                hit = true;
            }
        }
    };
    if (mesh.primitives.empty()) range(0, count, BackfaceCulling(mesh, 0, render, linear, resources), false);
    else for (std::size_t n = 0; n < mesh.primitives.size(); ++n) {
        const auto& part = mesh.primitives[n];
        if (std::find(render.hiddenParts.begin(), render.hiddenParts.end(), part.part) == render.hiddenParts.end())
            range(part.first, part.count, BackfaceCulling(mesh, n, render, linear, resources),
                  mirroredParts && n < mirroredParts->size() && (*mirroredParts)[n]);
    }
    return hit;
}

void PrepareMesh(MeshData& mesh, std::vector<bool>& mirroredParts) {
    if (mesh.skeletal && mesh.skinVertices.size() == mesh.vertices.size()) {
        const auto palette = ResolveSkinMatrices(mesh.skeletal->skeleton, mesh.skeletal->skeleton.rest);
        const auto skinMatrix = [&](const MeshSkinVertex& skin) {
            glm::mat4 matrix(0);
            for (int k = 0; k < 4; ++k) {
                if (skin.joints[k] < palette.size()) matrix += palette[skin.joints[k]] * skin.weights[k];
                if (skin.joints1[k] < palette.size()) matrix += palette[skin.joints1[k]] * skin.weights1[k];
            }
            return matrix;
        };
        // Renderer::DrawMesh corrects winding per primitive using the first
        // vertex's rest-skin matrix. Preserve that sign before discarding skin.
        mirroredParts.resize(mesh.primitives.size(), false);
        for (std::size_t n = 0; !palette.empty() && n < mesh.primitives.size(); ++n) {
            const auto& part = mesh.primitives[n];
            if (!part.count) continue;
            const auto count = mesh.indices.empty() ? mesh.vertices.size() : mesh.indices.size();
            if (part.first >= count) continue;
            const auto vertex = mesh.indices.empty() ? part.first : mesh.indices[part.first];
            if (vertex >= mesh.skinVertices.size()) continue;
            mirroredParts[n] = glm::determinant(glm::mat3(skinMatrix(mesh.skinVertices[vertex]))) < 0;
        }
        for (std::size_t n = 0; !palette.empty() && n < mesh.vertices.size(); ++n) {
            const auto matrix = skinMatrix(mesh.skinVertices[n]);
            mesh.vertices[n].position = glm::vec3(matrix * glm::vec4(mesh.vertices[n].position, 1));
        }
    }
    mesh.skeletal.reset();
    mesh.skinVertices.clear();
    for (auto& material : mesh.materials) material = MaterialSettings(material);
}
}

const MeshData* EditorScenePicker::AssetMesh(const std::string& id, bool collision, const AssetDatabase* assets) {
    if(!collision&&id==m_draftId&&m_draft)return m_draft.get();
    const auto* record = assets ? assets->Find(id) : nullptr;
    if (!record || record->missing || record->type != (collision ? AssetType::Collision : AssetType::Mesh)) return nullptr;
    std::error_code error;
    const auto modified = std::filesystem::last_write_time(record->path, error);
    if (error) return nullptr;
    const auto bytes = std::filesystem::file_size(record->path, error);
    if (error) return nullptr;
    auto found = m_meshes.find(id);
    if (found != m_meshes.end() && found->second.path == record->path &&
        found->second.modified == modified && found->second.bytes == bytes) return found->second.mesh.get();
    // Keep the click cache bounded across open projects. Callers can also
    // clear it on project changes to release abandoned entries immediately.
    if (found == m_meshes.end() && m_meshes.size() >= 64) m_meshes.erase(m_meshes.begin());
    CachedMesh entry;
    entry.path = record->path; entry.modified = modified; entry.bytes = bytes;
    auto mesh = std::make_shared<MeshData>();
    std::string reason;
    bool loaded = false;
    if (!collision) loaded = LoadModelMesh(record->path, *mesh, reason);
    else {
        CollisionAsset cooked;
        if (LoadCollisionAsset(record->path, cooked, reason)) {
            for (auto position : cooked.vertices) { MeshVertex vertex; vertex.position = glm::vec3(position); mesh->vertices.push_back(vertex); }
            for (const auto& face : cooked.faces) mesh->indices.insert(mesh->indices.end(), face.vertices.begin(), face.vertices.end());
            loaded = true;
        }
    }
    if (loaded) { PrepareMesh(*mesh, entry.mirroredParts); entry.mesh = std::move(mesh); }
    return m_meshes.insert_or_assign(id, std::move(entry)).first->second.mesh.get();
}

const MeshData* EditorScenePicker::TerrainMesh(const std::string& identifier) {
    const auto found = m_terrains.find(identifier);
    if (found != m_terrains.end()) return &found->second;
    const auto terrain = CreateTerrainSurface(identifier);
    if (!terrain) return nullptr;
    if (m_terrains.size() >= 8) m_terrains.erase(m_terrains.begin());
    // Same static authoring tessellation as ResourceManager::GetTerrainMesh.
    return &m_terrains.emplace(identifier, terrain->BuildMesh(96, 128)).first->second;
}

EditorPickHit EditorScenePicker::Pick(const Scene& authored, glm::vec3 origin, glm::vec3 direction,
                                     const AssetDatabase* assets, const ResourceManager* resources,
                                     std::string* error) {
    if (error) error->clear();
    EditorPickHit best;
    const double length = glm::length(glm::dvec3(direction));
    if (!Finite(origin) || !Finite(direction) || !(length > 1e-12)) return best;
    const Ray world{origin, glm::dvec3(direction) / length};
    Scene flat;
    std::string hierarchyError;
    if (!FlattenHierarchy(authored, flat, hierarchyError)) {
        if (error) *error = hierarchyError;
        return best;
    }
    const auto accept = [&](SceneObjectId id, double distance) {
        if (distance < best.distance) best = {id, float(distance), true};
    };
    for (const auto& object : flat.Objects()) {
        if (!(flat.Settings().mainCameraRenderMask & CategoryBit(object.renderLayer))) continue;
        const auto& transform = object.transform;
        const auto primitive = [&](glm::vec3 position, glm::quat rotation, glm::vec3 scale,
                                   glm::vec3 half, float radius, bool sphere) {
            Ray local; double distance = 0;
            if (LocalRay(world, position, rotation, scale, local) &&
                (sphere ? SphereHit(local, radius, distance) : BoxHit(local, -glm::dvec3(half), glm::dvec3(half), distance)))
                accept(object.id, distance);
        };
        if (object.render) {
            const auto& render = *object.render;
            if (render.shape == SceneShape::Mesh) {
                const bool placeholder = resources && resources->StateOf(render.meshAsset) != ResourceState::Ready;
                if (placeholder || (!assets && !resources))
                    primitive(transform.position, transform.rotation, transform.scale, glm::vec3(0.5f), 0, false);
                else if (const auto* mesh = AssetMesh(render.meshAsset, false, assets)) {
                    Ray local; double distance = 0;
                    if (LocalRay(world, transform.position, transform.rotation, transform.scale, local) &&
                        MeshHit(*mesh, local, render, flat.Settings().linearRendering, resources, distance,
                                &m_meshes.at(render.meshAsset).mirroredParts)) accept(object.id, distance);
                }
            } else if (render.shape == SceneShape::Compound && object.body) {
                for (const auto& child : object.body->compoundBoxes) {
                    const auto position = transform.position + glm::normalize(transform.rotation) * child.localCenter;
                    const auto rotation = glm::normalize(transform.rotation * child.rotation);
                    if (child.type == ShapeType::Box || child.type == ShapeType::Sphere)
                        primitive(position, rotation, glm::vec3(1), child.halfExtents, child.radius, child.type == ShapeType::Sphere);
                    else if (child.type == ShapeType::ConvexHull) {
                        if (resources && resources->StateOf(child.assetId) != ResourceState::Ready) continue;
                        const auto* mesh = AssetMesh(child.assetId, true, assets);
                        Ray local; double distance = 0;
                        if (mesh && LocalRay(world, position, rotation, glm::vec3(1), local) &&
                            MeshHit(*mesh, local, render, false, nullptr, distance)) accept(object.id, distance);
                    }
                }
            } else if (render.shape == SceneShape::Terrain && object.body) {
                const auto* mesh = TerrainMesh(object.body->terrainSurface);
                Ray local; double distance = 0;
                if (mesh && LocalRay(world, transform.position, transform.rotation, glm::vec3(1), local) &&
                    MeshHit(*mesh, local, render, false, nullptr, distance)) accept(object.id, distance);
            } else if (object.door || object.lightSwitch) {
                primitive(transform.position + glm::normalize(transform.rotation) * glm::vec3(render.halfExtents.x, 0, 0),
                          transform.rotation, glm::vec3(1), render.halfExtents, 0, false);
            } else if (render.shape == SceneShape::Box || render.shape == SceneShape::Sphere) {
                primitive(transform.position, transform.rotation, transform.scale,
                          render.halfExtents, render.radius, render.shape == SceneShape::Sphere);
            }
        }
        if (object.fluidVolume) {
            const auto& fluid = *object.fluidVolume;
            const glm::vec3 half = 0.5f * fluid.spacing * glm::vec3(fluid.countX, fluid.countY, fluid.countZ);
            primitive(transform.position + glm::normalize(transform.rotation) * glm::vec3(0, half.y - 0.5f * fluid.spacing, 0),
                      transform.rotation, glm::vec3(1), half, 0, false);
        }
    }
    // Empty/light/body-only objects remain accessible through a small pivot
    // proxy, but cannot steal any actual rendered surface on this ray.
    if (best.id != kInvalidSceneObjectId) return best;
    for (const auto& object : flat.Objects()) {
        if (object.render || object.fluidVolume) continue;
        const Ray local{world.origin - glm::dvec3(object.transform.position), world.direction};
        double distance = 0;
        if (SphereHit(local, 0.25, distance) && distance < best.distance)
            best = {object.id, float(distance), false};
    }
    return best;
}

void EditorScenePicker::Clear() { m_meshes.clear(); m_terrains.clear();m_draft.reset();m_draftId.clear(); }
