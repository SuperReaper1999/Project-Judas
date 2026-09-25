#include "WorldDebugView.h"

#include <algorithm>
#include <cmath>

#include <glm/gtc/quaternion.hpp>

#include "GameSession.h"
#include "Interactable.h"
#include "PlayerController.h"
#include "RadialTerrain.h"
#include "RuntimeWorld.h"
#include "Scene.h"
#include "WorldPresentation.h"

namespace {
const glm::vec3 kStaticShapeColor(0.35f, 0.9f, 0.4f);
const glm::vec3 kDynamicShapeColor(0.95f, 0.6f, 0.2f);
const glm::vec3 kTerrainColor(0.5f, 0.8f, 0.5f);
const glm::vec3 kCapsuleColor(0.2f, 0.7f, 1.0f);
const glm::vec3 kGroundedColor(0.2f, 1.0f, 0.3f);
const glm::vec3 kAirborneColor(1.0f, 0.3f, 0.2f);
const glm::vec3 kContactColor(1.0f, 0.2f, 0.9f);
const glm::vec3 kGravityColor(0.9f, 0.9f, 0.2f);
const glm::vec3 kRegionColor(0.6f, 0.6f, 0.2f);
const glm::vec3 kLightColor(1.0f, 0.95f, 0.6f);
const glm::vec3 kInteractColor(0.3f, 0.9f, 0.9f);
const glm::vec3 kInteractTargetColor(1.0f, 1.0f, 1.0f);
const glm::vec3 kNormalColor(0.7f, 1.0f, 0.7f);
const glm::vec3 kFluidColor(0.3f, 0.6f, 1.0f);
const glm::vec3 kAtmosphereColor(0.6f, 0.75f, 1.0f);
const glm::vec3 kPlayerStartColor(0.2f, 0.9f, 1.0f);

glm::vec3 FidelityColor(const EntityRecord& e) {
    if (e.lifecycle == EntityLifecycle::Destroyed) return glm::vec3(0.9f, 0.15f, 0.15f);
    switch (e.fidelity) {
        case SimulationFidelity::Full: return glm::vec3(0.2f, 1.0f, 0.3f);
        case SimulationFidelity::Coarse: return glm::vec3(1.0f, 0.9f, 0.2f);
        case SimulationFidelity::Dormant: return glm::vec3(0.55f, 0.55f, 0.55f);
    }
    return glm::vec3(1.0f);
}

void DrawBodyShape(DebugLineList& out, const SceneBodyComponent& body, const glm::vec3& position,
                   const glm::quat& rotation, const glm::vec3& color) {
    switch (body.shape) {
        case SceneShape::Box:
            out.Box(position, rotation, body.halfExtents, color);
            break;
        case SceneShape::Sphere:
            out.Sphere(position, body.radius, color);
            break;
        case SceneShape::Compound:
            for (const CompoundBox& child : body.compoundBoxes) {
                out.Box(position + rotation * child.localCenter, rotation, child.halfExtents, color);
            }
            break;
        case SceneShape::Terrain:
        case SceneShape::Mesh:
            break;
    }
}

void DrawGravityRegion(DebugLineList& out, const SceneGravityComponent& g, const glm::vec3& position,
                       const glm::quat& rotation) {
    if (g.regionShape == SceneRegionShape::Sphere) {
        out.Sphere(position, g.regionRadius, kRegionColor, 32);
    } else {
        out.Box(position, glm::quat(1.0f, 0.0f, 0.0f, 0.0f), g.regionHalfExtents, kRegionColor);
    }
    if (g.kind == SceneGravityKind::Radial) {
        out.Cross(position, 0.5f, kGravityColor);
    } else {
        const glm::vec3 down = rotation * glm::vec3(0.0f, -1.0f, 0.0f);
        out.Arrow(position, position + down * 2.0f, kGravityColor, 0.3f);
    }
}

void DrawLight(DebugLineList& out, const DynamicLight& light) {
    if (light.kind == LightKind::Point) {
        out.Cross(light.position, 0.25f, kLightColor);
        out.Sphere(light.position, light.range, kLightColor, 24);
        return;
    }
    const glm::vec3 d = glm::length(light.direction) > 1.0e-6f ? glm::normalize(light.direction)
                                                              : glm::vec3(0.0f, 0.0f, -1.0f);
    const float outer = glm::radians(light.outerConeDegrees);
    const float coneRadius = std::tan(outer) * light.range;
    const glm::vec3 end = light.position + d * light.range;
    out.Circle(end, d, coneRadius, kLightColor, 24);
    const glm::vec3 helper = std::abs(d.y) < 0.9f ? glm::vec3(0.0f, 1.0f, 0.0f) : glm::vec3(1.0f, 0.0f, 0.0f);
    const glm::vec3 u = glm::normalize(glm::cross(helper, d));
    const glm::vec3 v = glm::cross(d, u);
    for (const glm::vec3& side : {u, -u, v, -v}) out.Line(light.position, end + side * coneRadius, kLightColor);
    out.Cross(light.position, 0.25f, kLightColor);
}
}  // namespace

void BuildWorldDebugLines(const RuntimeWorld& world, const GameSession* session, float alpha,
                          const DebugViewOptions& options, DebugLineList& out) {
    if (!world.IsBuilt() || !options.AnyEnabled()) return;
    const std::vector<DynamicBody>& bodies = world.DynamicBodies();

    // Positions of every entity this frame, live or retained.
    const auto entityPose = [&](const EntityRecord& e, glm::vec3& position, glm::quat& rotation) {
        if (e.slot < bodies.size() && bodies[e.slot].IsLive()) {
            position = bodies[e.slot].GetPresentedPosition(alpha);
            rotation = bodies[e.slot].GetPresentedOrientation(alpha);
        } else {
            position = e.state.position;
            rotation = e.state.rotation;
        }
    };

    if (options.collisionShapes) {
        for (const RuntimeWorld::StaticBody& sb : world.StaticBodies()) {
            if (sb.shape == SceneShape::Box) out.Box(sb.position, sb.rotation, sb.halfExtents, kStaticShapeColor);
            else if (sb.shape == SceneShape::Sphere) out.Sphere(sb.position, sb.radius, kStaticShapeColor);
        }
        for (const RuntimeWorld::Terrain& t : world.Terrains()) {
            if (t.surface) out.Sphere(t.position, t.surface->BoundRadius(), kTerrainColor, 48);
        }
        for (const EntityRecord& e : world.Entities()) {
            if (e.lifecycle == EntityLifecycle::Destroyed || !e.definition.body) continue;
            if (e.slot >= bodies.size() || !bodies[e.slot].IsLive()) continue;  // no physics shape exists
            glm::vec3 position; glm::quat rotation;
            entityPose(e, position, rotation);
            DrawBodyShape(out, *e.definition.body, position, rotation, kDynamicShapeColor);
        }
    }

    if (options.playerCapsule && session) {
        const PlayerController& player = session->Player();
        const glm::vec3 position = player.GetPresentedPosition(alpha);
        const glm::quat orientation = player.GetPresentedOrientation(alpha);
        out.Capsule(position, orientation, PlayerController::CapsuleRadius(), PlayerController::CapsuleHalfHeight(),
                    kCapsuleColor);
        const glm::vec3 up = orientation * glm::vec3(0.0f, 1.0f, 0.0f);
        const glm::vec3 foot = position - up * (PlayerController::CapsuleHalfHeight() + PlayerController::CapsuleRadius());
        out.Circle(foot, up, PlayerController::CapsuleRadius() * 1.2f,
                   player.IsGrounded() ? kGroundedColor : kAirborneColor, 16);
        out.Arrow(position, position + player.GetLookDirection() * 1.5f, kCapsuleColor, 0.2f);
    }

    if (options.contacts) {
        for (const PhysicsWorld::DebugContact& c : world.Physics().LastStepContacts()) {
            out.Cross(c.point, 0.06f, kContactColor);
            out.Arrow(c.point, c.point + c.normal * 0.4f, kContactColor, 0.08f);
        }
        if (session && session->Player().IsGrounded()) {
            const BodyHandle support = session->Player().GetSupportBodyHandle();
            if (support.IsValid()) {
                const BodyTransform t = world.Physics().GetTransform(support);
                out.Line(session->Player().GetPresentedPosition(alpha), t.position, kGroundedColor);
            }
        }
    }

    if (options.gravity) {
        const auto arrow = [&](const glm::vec3& at) {
            const glm::vec3 g = world.Gravity().Sample(at);
            const float magnitude = glm::length(g);
            if (magnitude < 1.0e-4f) { out.Cross(at, 0.15f, kGravityColor); return; }
            out.Arrow(at, at + g / magnitude * std::min(2.0f, 0.15f * magnitude + 0.5f), kGravityColor, 0.2f);
        };
        for (const DynamicBody& body : bodies) {
            if (body.IsLive()) arrow(body.GetPresentedPosition(alpha));
        }
        if (session) arrow(session->Player().GetPresentedPosition(alpha));
        for (const RuntimeWorld::GravityRegion& region : world.GravityRegions()) {
            DrawGravityRegion(out, region.component, region.position, region.rotation);
        }
    }

    if (options.frameAxes) {
        out.Axes(glm::vec3(0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), 2.0f);
        for (const RuntimeWorld::StaticBody& sb : world.StaticBodies()) out.Axes(sb.position, sb.rotation, 0.6f);
        for (const RuntimeWorld::Terrain& t : world.Terrains()) out.Axes(t.position, t.rotation, 5.0f);
        for (const DynamicBody& body : bodies) {
            if (body.IsLive()) out.Axes(body.GetPresentedPosition(alpha), body.GetPresentedOrientation(alpha), 0.6f);
        }
        if (session) {
            out.Axes(session->Player().GetPresentedPosition(alpha), session->Player().GetPresentedOrientation(alpha), 0.8f);
        }
    }

    if (options.lights) {
        for (const DynamicLight& light : BuildWorldLights(world, session, alpha)) DrawLight(out, light);
    }

    if (options.interactionRanges && session) {
        const Interactable* target = session->InteractionTarget();
        for (const Interactable* interactable : session->Interactables()) {
            if (!interactable) continue;
            const bool isTarget = interactable == target;
            out.Sphere(interactable->GetInteractionPoint(), interactable->GetInteractionRadius(),
                       isTarget ? kInteractTargetColor : kInteractColor, isTarget ? 24 : 12);
        }
        const BodyHandle held = session->Manipulation().HeldBody();
        if (held.IsValid()) {
            const BodyTransform t = world.Physics().GetTransform(held);
            out.Line(session->Player().GetPresentedPosition(alpha), t.position, kInteractTargetColor);
        }
    }

    if (options.lifecycle) {
        for (const EntityRecord& e : world.Entities()) {
            glm::vec3 position; glm::quat rotation;
            entityPose(e, position, rotation);
            const glm::vec3 color = FidelityColor(e);
            out.Cross(position, 0.35f, color);
            if (e.fidelity != SimulationFidelity::Full || e.lifecycle == EntityLifecycle::Destroyed) {
                out.Circle(position, glm::vec3(0.0f, 1.0f, 0.0f), 0.45f, color, 12);
            }
        }
    }

    if (options.terrainNormals) {
        for (const RuntimeWorld::Terrain& t : world.Terrains()) {
            if (!t.surface) continue;
            // Sample around the player when there is one, else around the
            // terrain's local +Y pole: 8 rings x 16 directions.
            glm::vec3 centreLocal = glm::vec3(0.0f, 1.0f, 0.0f);
            if (session) {
                const glm::vec3 local = glm::inverse(t.rotation) * (session->Player().GetPresentedPosition(alpha) - t.position);
                if (glm::length(local) > 1.0e-3f) centreLocal = glm::normalize(local);
            }
            const glm::vec3 helper = std::abs(centreLocal.y) < 0.9f ? glm::vec3(0.0f, 1.0f, 0.0f) : glm::vec3(1.0f, 0.0f, 0.0f);
            const glm::vec3 u = glm::normalize(glm::cross(helper, centreLocal));
            const glm::vec3 v = glm::cross(centreLocal, u);
            for (int ring = 0; ring <= 8; ++ring) {
                const float angle = static_cast<float>(ring) * 0.02f;  // radians of arc from centre
                const int count = ring == 0 ? 1 : 16;
                for (int i = 0; i < count; ++i) {
                    const float phi = static_cast<float>(i) / static_cast<float>(count) * 6.2831853f;
                    const glm::vec3 direction = glm::normalize(centreLocal * std::cos(angle) +
                                                               (u * std::cos(phi) + v * std::sin(phi)) * std::sin(angle));
                    const TerrainSample sample = t.surface->Sample(direction * t.surface->BoundRadius());
                    const glm::vec3 point = t.position + t.rotation * sample.surfacePoint;
                    const glm::vec3 normal = t.rotation * sample.outwardNormal;
                    out.Arrow(point, point + normal * 1.0f, kNormalColor, 0.12f);
                }
            }
        }
    }

    if (options.fluidParticles && world.HasFluid()) {
        const FluidWorld& fluid = world.Fluid();
        const std::size_t count = std::min(fluid.Particles().size(), kMaxFluidParticleMarkers);
        const float size = std::max(0.01f, world.FluidSettingsUsed().particleRadius * 0.5f);
        for (std::size_t i = 0; i < count; ++i) out.Cross(fluid.PresentedPosition(i, alpha), size, kFluidColor);
    }

    if (options.atmosphere && world.GetAtmosphere()) {
        const RuntimeWorld::Atmosphere& a = *world.GetAtmosphere();
        const glm::vec3 centre = a.frame.originPosition;
        out.Sphere(centre, a.field.Parameters().referenceRadius, kAtmosphereColor, 48);
        out.Sphere(centre, a.field.Parameters().topRadius, kAtmosphereColor * 0.6f, 48);
    }
}

void BuildAuthoredDebugLines(const Scene& scene, const DebugViewOptions& options, DebugLineList& out) {
    if (!options.AnyEnabled()) return;
    if (options.frameAxes) out.Axes(glm::vec3(0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), 2.0f);
    for (const SceneObject& o : scene.Objects()) {
        const glm::vec3 position = o.transform.position;
        const glm::quat rotation = glm::normalize(o.transform.rotation);
        if (options.collisionShapes && o.body) {
            const bool dynamic = o.body->motion == SceneBodyMotion::Dynamic;
            DrawBodyShape(out, *o.body, position, rotation, dynamic ? kDynamicShapeColor : kStaticShapeColor);
            if (o.body->shape == SceneShape::Terrain) out.Cross(position, 2.0f, kTerrainColor);
        }
        if (options.collisionShapes && o.door && o.render) {
            out.Box(position + rotation * glm::vec3(o.render->halfExtents.x, 0.0f, 0.0f), rotation, o.render->halfExtents,
                    kStaticShapeColor);
        }
        if (options.gravity && o.gravity) DrawGravityRegion(out, *o.gravity, position, rotation);
        if (options.frameAxes && (o.body || o.render || o.light || o.playerStart)) out.Axes(position, rotation, 0.6f);
        if (options.lights && o.light) {
            DynamicLight light;
            light.kind = o.light->kind == SceneLightKind::Spot ? LightKind::Spot : LightKind::Point;
            light.position = position;
            light.direction = rotation * glm::vec3(0.0f, 0.0f, -1.0f);
            light.range = o.light->range;
            light.outerConeDegrees = o.light->outerConeDegrees;
            DrawLight(out, light);
        }
        if (options.lights && o.lightSwitch) {
            out.Cross(position + rotation * o.lightSwitch->lampLocalOffset, 0.25f, kLightColor);
        }
        if (options.playerCapsule && o.playerStart) {
            const glm::quat facing = glm::angleAxis(glm::radians(o.playerStart->yawDegrees), glm::vec3(0.0f, 1.0f, 0.0f));
            out.Capsule(position, glm::quat(1.0f, 0.0f, 0.0f, 0.0f), PlayerController::CapsuleRadius(),
                        PlayerController::CapsuleHalfHeight(), kPlayerStartColor);
            out.Arrow(position, position + facing * glm::vec3(0.0f, 0.0f, -1.5f), kPlayerStartColor, 0.2f);
        }
        if (options.atmosphere && o.atmosphere) {
            out.Sphere(position, o.atmosphere->referenceRadius, kAtmosphereColor, 48);
            out.Sphere(position, o.atmosphere->topRadius, kAtmosphereColor * 0.6f, 48);
        }
        if (options.fluidParticles && o.fluidVolume) {
            const SceneFluidVolumeComponent& f = *o.fluidVolume;
            const glm::vec3 half(0.5f * f.spacing * static_cast<float>(f.countX), 0.5f * f.spacing * static_cast<float>(f.countY),
                                 0.5f * f.spacing * static_cast<float>(f.countZ));
            out.Box(position + rotation * glm::vec3(0.0f, half.y - 0.5f * f.spacing, 0.0f), rotation, half, kFluidColor);
        }
    }
}
