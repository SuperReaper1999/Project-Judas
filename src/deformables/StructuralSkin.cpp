#include "StructuralSkin.h"
#include "PanelIslands.h"
#include "Deformable.h"
#include "PerformanceProfiler.h"
#include "SkeletalAnimation.h"
#include "StructuralMass.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace {
glm::dmat3 Basis(glm::dvec3 a, glm::dvec3 b, glm::dvec3 c) {
    auto x = glm::normalize(b - a), z = glm::normalize(glm::cross(x, c - a));
    return {x, glm::cross(z, x), z};
}
} // namespace
bool StructuralSkin::Bind(const DeformableAsset &asset, const MeshData &mesh, std::string &error, bool panelSeams) {
    if (!asset.fracture || !asset.fracture->rigid || asset.fracture->parts.empty() ||
        asset.fracture->parts.size() > 256 || mesh.indices.empty() || mesh.indices.size() % 3 ||
        mesh.indices.size() > 3000000) {
        error = "structural skin requires a prepared rigid partition and bounded indexed triangles";
        return false;
    }
    StructuralSkin next;
    for (const auto &part : asset.fracture->parts) {
        next.centers.push_back(IntegrateStructuralMass(asset, part, 1).center);
        auto tet = asset.tetrahedra[part.tets.front()];
        Frame f;
        f.nodes = {tet.x, tet.y, tet.z};
        f.inverse = glm::transpose(Basis(asset.nodes[tet.x], asset.nodes[tet.y], asset.nodes[tet.z]));
        next.frames.push_back(f);
    }
    next.rest = mesh;
    next.rest.vertices.clear();
    next.rest.indices.clear();
    next.rest.skinVertices.clear();
    next.rest.skeletal.reset();
    if (next.rest.primitives.empty())
        next.rest.primitives.push_back({0, unsigned(mesh.indices.size()), -1});
    for (const auto &primitive : next.rest.primitives) {
        if (primitive.first > mesh.indices.size() ||
            primitive.count > mesh.indices.size() - primitive.first || primitive.first % 3 ||
            primitive.count % 3) {
            error = "invalid structural skin primitive range";
            return false;
        }
    }
    auto nearest = [&](glm::dvec3 p) {
        unsigned best = 0;
        double distance = 1e100;
        for (unsigned i = 0; i < next.centers.size(); ++i) {
            auto d = p - next.centers[i];
            double value = glm::dot(d, d);
            if (value < distance) {
                distance = value;
                best = i;
            }
        }
        return best;
    };
    std::vector<unsigned> panelGroups;
    std::map<unsigned,glm::dvec3> panelCenters;
    std::map<unsigned,double> panelAreas;
    if(panelSeams){
        panelGroups=MeshPanelIslands(mesh);
        for(unsigned t=0;t<mesh.indices.size();t+=3){auto p=glm::dvec3(mesh.vertices[mesh.indices[t]].position),q=glm::dvec3(mesh.vertices[mesh.indices[t+1]].position),r=glm::dvec3(mesh.vertices[mesh.indices[t+2]].position);
            auto group=panelGroups[t/3];double area=glm::length(glm::cross(q-p,r-p));
            if(!panelCenters.count(group))panelCenters[group]=glm::dvec3(0);
            panelCenters[group]+=(p+q+r)*(area/3);panelAreas[group]+=area;
        }
        for(auto& [group,center]:panelCenters)if(panelAreas[group]>0)center/=panelAreas[group];
    }
    for (size_t face = 0; face < mesh.indices.size(); face += 3) {
        glm::dvec3 centroid(0);
        for (unsigned k = 0; k < 3; ++k) {
            if (mesh.indices[face + k] >= mesh.vertices.size()) {
                error = "structural skin index out of range";
                return false;
            }
            centroid += glm::dvec3(mesh.vertices[mesh.indices[face + k]].position) / 3.;
        }
        const auto owner = nearest(panelSeams ? panelCenters.at(panelGroups[face/3]) : centroid);
        for (unsigned k = 0; k < 3; ++k) {
            const auto vertex = mesh.vertices[mesh.indices[face + k]];
            if (!std::isfinite(vertex.position.x) || !std::isfinite(vertex.position.y) ||
                !std::isfinite(vertex.position.z)) {
                error = "nonfinite structural skin position";
                return false;
            }
            std::vector<std::pair<double, unsigned>> candidates;
            for (unsigned p = 0; p < next.centers.size(); ++p) {
                auto d = glm::dvec3(vertex.position) - next.centers[p];
                candidates.push_back({glm::dot(d, d), p});
            }
            std::sort(candidates.begin(), candidates.end());
            Corner corner;
            corner.owner = owner;
            double total = 0;
            for (unsigned j = 0; j < std::min<size_t>(4, candidates.size()); ++j) {
                corner.parts[j] = candidates[j].second;
                corner.weights[j] = 1 / std::pow(std::max(.04, candidates[j].first), 2);
                total += corner.weights[j];
            }
            for (auto &weight : corner.weights)
                weight /= total;
            next.corners.push_back(corner);
            next.rest.indices.push_back(unsigned(next.rest.vertices.size()));
            next.rest.vertices.push_back(vertex);
        }
    }
    *this = std::move(next);
    error.clear();
    return true;
}
void StructuralSkin::Map(const DeformableInstance &simulation, double alpha, MeshData &mesh) const {
    JUDAS_PROFILE_SCOPE("Structural detailed skin mapping");
    if (!simulation.asset->fracture || simulation.asset->fracture->parts.size() != frames.size())
        throw std::invalid_argument("structural skin partition changed");
    if (mesh.vertices.size() != rest.vertices.size())
        mesh = rest;
    alpha = std::clamp(alpha, 0., 1.);
    std::vector<glm::dmat3> rotations;
    std::vector<glm::dvec3> translations;
    for (unsigned i = 0; i < frames.size(); ++i) {
        auto n = frames[i].nodes;
        auto a = simulation.positions[n.x], b = simulation.positions[n.y], c = simulation.positions[n.z];
        auto r = Basis(a, b, c) * frames[i].inverse;
        auto previous = Basis(simulation.previous[n.x], simulation.previous[n.y], simulation.previous[n.z]) *
                        frames[i].inverse;
        auto rotation = glm::mat3_cast(glm::slerp(glm::quat_cast(previous), glm::quat_cast(r), alpha));
        auto position = glm::mix(simulation.previous[n.x], a, alpha);
        rotations.push_back(rotation);
        translations.push_back(position - rotation * simulation.asset->nodes[n.x]);
    }
    for (size_t i = 0; i < corners.size(); ++i) {
        const auto &corner = corners[i];
        auto &out = mesh.vertices[i];
        const auto &source = rest.vertices[i];
        glm::dvec3 position(0), normal(0), tangent(0);
        double sum = 0;
        for (unsigned j = 0; j < 4; ++j) {
            auto p = corner.parts[j];
            if (corner.weights[j] == 0 ||
                simulation.fracture.component[p] != simulation.fracture.component[corner.owner])
                continue;
            double w = corner.weights[j];
            position += w * (rotations[p] * glm::dvec3(source.position) + translations[p]);
            normal += w * rotations[p] * glm::dvec3(source.normal);
            tangent += w * rotations[p] * glm::dvec3(source.tangent);
            sum += w;
        }
        if (sum == 0) {
            auto p = corner.owner;
            position = rotations[p] * glm::dvec3(source.position) + translations[p];
            normal = rotations[p] * glm::dvec3(source.normal);
            tangent = rotations[p] * glm::dvec3(source.tangent);
            sum = 1;
        }
        out.position = glm::vec3(position / sum);
        out.normal = glm::length(normal) > 1e-12 ? glm::vec3(glm::normalize(normal)) : source.normal;
        out.tangent = glm::vec4(glm::length(tangent) > 1e-12 ? glm::vec3(glm::normalize(tangent))
                                                             : glm::vec3(source.tangent),
                                source.tangent.w);
    }
    // Triangles have one owner; disconnected/removed sections cannot retain a
    // bridge made out of vertices blending two independently moving fragments.
    mesh.indices.clear();
    mesh.primitives.clear();
    for (const auto &primitive : rest.primitives) {
        unsigned first = unsigned(mesh.indices.size());
        for (unsigned t = primitive.first; t < primitive.first + primitive.count; t += 3)
            if (!simulation.fracture.removed[corners[t].owner])
                for (unsigned k = 0; k < 3; ++k)
                    mesh.indices.push_back(t + k);
        if (mesh.indices.size() > first)
            mesh.primitives.push_back({first, unsigned(mesh.indices.size()) - first, primitive.material,
                                       primitive.part, primitive.node});
    }
    if (mesh.indices.empty())
        mesh.vertices.clear();
}
size_t StructuralSkin::Bytes() const {
    size_t bytes=rest.vertices.size() * sizeof(MeshVertex) + rest.indices.size() * sizeof(unsigned) +
           corners.size() * sizeof(Corner) + frames.size() * sizeof(Frame) +
           centers.size() * sizeof(glm::dvec3);
    for(const auto& material:rest.materials)for(const auto& map:material.maps){
        bytes+=map.encodedImage.size()+map.embedded.pixels.size();
        for(const auto& level:map.embedded.levels)bytes+=level.bytes.size();
    }
    return bytes;
}

void StructuralSkin::Palette(const DeformableInstance &simulation, double alpha,
                             std::vector<glm::mat4> &palette) const {
    JUDAS_PROFILE_SCOPE("Structural skin physical palette");
    if (!simulation.asset->fracture || simulation.asset->fracture->parts.size() != frames.size())
        throw std::invalid_argument("structural skin partition changed");
    alpha = std::clamp(alpha, 0., 1.);
    palette.resize(frames.size());
    for (unsigned i = 0; i < frames.size(); ++i) {
        auto n = frames[i].nodes;
        auto r = Basis(simulation.positions[n.x], simulation.positions[n.y], simulation.positions[n.z]) *
                 frames[i].inverse;
        auto before = Basis(simulation.previous[n.x], simulation.previous[n.y], simulation.previous[n.z]) *
                      frames[i].inverse;
        auto rotation = glm::mat3_cast(glm::slerp(glm::quat_cast(before), glm::quat_cast(r), alpha));
        auto position = glm::mix(simulation.previous[n.x], simulation.positions[n.x], alpha);
        auto transform = glm::dmat4(rotation);
        transform[3] = glm::dvec4(position - rotation * simulation.asset->nodes[n.x], 1);
        palette[i] = glm::mat4(transform);
    }
}
void StructuralSkin::BuildGpuMesh(const DeformableInstance &simulation, MeshData &mesh) const {
    if (!simulation.asset->fracture || simulation.asset->fracture->parts.size() != frames.size())
        throw std::invalid_argument("structural skin partition changed");
    // Rebuild only on topology publication. Ordinary motion updates the bounded
    // palette, never the exterior vertex buffer. This uses the existing renderer.
    mesh = rest;
    mesh.skinVertices.resize(corners.size());
    auto asset = std::make_shared<SkeletalAsset>();
    auto &skeleton = asset->skeleton;
    for (unsigned p = 0; p < frames.size(); ++p) {
        skeleton.names.push_back("section-" + std::to_string(p));
        skeleton.parents.push_back(-1);
        skeleton.order.push_back(int(p));
        skeleton.rest.local.emplace_back();
        skeleton.skinNodes.push_back(int(p));
        skeleton.inverseBind.emplace_back(1);
    }
    mesh.skeletal = std::move(asset);
    for (size_t i = 0; i < corners.size(); ++i) {
        const auto &corner = corners[i];
        auto &skin = mesh.skinVertices[i];
        skin.weights = glm::vec4(0);
        double sum = 0;
        for (unsigned k = 0; k < 4; ++k) {
            auto p = corner.parts[k];
            skin.joints[k] = p;
            if (simulation.fracture.component[p] == simulation.fracture.component[corner.owner]) {
                skin.weights[k] = float(corner.weights[k]);
                sum += corner.weights[k];
            }
        }
        if (sum > 0)
            skin.weights /= float(sum);
        else {
            skin.joints = glm::uvec4(corner.owner);
            skin.weights = {1, 0, 0, 0};
        }
    }
    mesh.indices.clear();
    mesh.primitives.clear();
    for (const auto &primitive : rest.primitives) {
        unsigned first = unsigned(mesh.indices.size());
        for (unsigned t = primitive.first; t < primitive.first + primitive.count; t += 3)
            if (!simulation.fracture.removed[corners[t].owner])
                for (unsigned k = 0; k < 3; ++k)
                    mesh.indices.push_back(t + k);
        if (mesh.indices.size() > first)
            mesh.primitives.push_back({first, unsigned(mesh.indices.size()) - first, primitive.material,
                                       primitive.part, primitive.node});
    }
    if (mesh.indices.empty()) {
        mesh.vertices.clear();
        mesh.skinVertices.clear();
        mesh.skeletal.reset();
    }
}
