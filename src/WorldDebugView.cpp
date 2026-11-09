#include "CollisionAsset.h"
#include "WorldDebugView.h"
#include "CollisionGeometry.h"

#include <algorithm>
#include <cmath>

#include <glm/gtc/quaternion.hpp>

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
const glm::vec3 kBroadphaseColor(0.55f, 0.55f, 0.95f);

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
        case SceneShape::Planar: {
            if(!body.planar)break;
            const auto& c=*body.planar;
            auto line=[&](glm::dvec2 a,glm::dvec2 b){out.Line(position+rotation*glm::vec3(a,0),position+rotation*glm::vec3(b,0),color);};
            if(c.radius>0) {
                const double pi=3.141592653589793;
                if(c.kind==PlanarKind::Circle)for(int k=0;k<32;++k){double a=k*2*pi/32,b=(k+1)*2*pi/32;line(c.radius*glm::dvec2(cos(a),sin(a)),c.radius*glm::dvec2(cos(b),sin(b)));}
                else {const double h=c.vertices[1].y;
                    line({-c.radius,-h},{-c.radius,h});line({c.radius,-h},{c.radius,h});
                    for(int side:{-1,1})for(int k=0;k<16;++k){double a=k*pi/16,b=(k+1)*pi/16;
                        line({c.radius*cos(a),side*(h+c.radius*sin(a))},{c.radius*cos(b),side*(h+c.radius*sin(b))});}}
            } else {for(size_t i=0;i+1<c.vertices.size();++i)line(c.vertices[i],c.vertices[i+1]);
                if(c.kind!=PlanarKind::Chain)line(c.vertices.back(),c.vertices.front());}
            break;
        }
        case SceneShape::Sprite: break; // visual-only, rejected as a body

        case SceneShape::Box:
            out.Box(position, rotation, body.halfExtents, color);
            break;
        case SceneShape::Sphere:
            out.Sphere(position, body.radius, color);
            break;
        case SceneShape::Compound:
            for (const CompoundBox& child : body.compoundBoxes) {
                if(child.type==ShapeType::Box)out.Box(position + rotation * child.localCenter, rotation*child.rotation, child.halfExtents, color);else if(child.type==ShapeType::Sphere)out.Sphere(position+rotation*child.localCenter,child.radius,color);
            }
            break;
        case SceneShape::Mesh:
        case SceneShape::ConvexHull:
        case SceneShape::TriangleMesh:
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

void BuildWorldDebugLines(const RuntimeWorld& world, float alpha,
                          const DebugViewOptions& options, DebugLineList& out) {
    if (!world.IsBuilt() || !options.AnyEnabled()) return;
    if(options.navigation)world.Navigation().Debug(out);
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
        for(auto& definition:world.ScriptObjects())if(definition.body){Shape shape;BodyTransform pose;if(!world.Physics().GetBodyShape(world.RuntimeBody(definition.id),shape,pose))continue;RigidBody parent;parent.position=pose.position;parent.orientation=pose.rotation;
            for(int i=0;i<PrimitiveCount(shape);++i){auto primitive=PrimitiveAt(shape,parent,i);if(!primitive.shape.asset)continue;auto& asset=*primitive.shape.asset;auto point=[&](glm::dvec3 p){return glm::vec3(PrimitiveCenter(primitive)+PrimitiveRotation(primitive)*p);};for(auto& face:asset.faces){for(unsigned k=0;k<3;++k)out.Line(point(asset.vertices[face.vertices[k]]),point(asset.vertices[face.vertices[(k+1)%3]]),face.active[k]?kDynamicShapeColor:kStaticShapeColor);auto center=(asset.vertices[face.vertices[0]]+asset.vertices[face.vertices[1]]+asset.vertices[face.vertices[2]])/3.;out.Line(point(center),point(center+face.normal*.12),kNormalColor);}}
        }

        for (const RuntimeWorld::StaticBody& sb : world.StaticBodies()) {
            if (sb.shape == SceneShape::Box) out.Box(sb.position, sb.rotation, sb.halfExtents, kStaticShapeColor);
            else if (sb.shape == SceneShape::Sphere) out.Sphere(sb.position, sb.radius, kStaticShapeColor);
        }

        for (const EntityRecord& e : world.Entities()) {
            if (e.lifecycle == EntityLifecycle::Destroyed || !e.definition.body) continue;
            if (e.slot >= bodies.size() || !bodies[e.slot].IsLive()) continue;  // no physics shape exists
            glm::vec3 position; glm::quat rotation;
            entityPose(e, position, rotation);
            DrawBodyShape(out, *e.definition.body, position, rotation, kDynamicShapeColor);
        }
    }



    if (options.playerCapsule) for(const auto& object:world.ScriptObjects()) {
        if(!object.characterMotor)continue;
        const auto pose=world.PresentedTransform(object.id,object.transform,alpha);
        const auto& motor=*object.characterMotor;
        out.Capsule(pose.position+pose.rotation*motor.offset,pose.rotation,
                    motor.radius,motor.halfHeight,kCapsuleColor);
    }

    if (options.broadphase) {
        for (const BodyHandle handle : world.Physics().AliveBodies()) {
            glm::vec3 low, high;
            if (!world.Physics().GetBodyBroadphaseBounds(handle, low, high)) continue;
            // Terrain bounds are planet-sized; draw them only when near.
            if (glm::length(high - low) > 200.0f) continue;
            out.Box(0.5f * (low + high), glm::quat(1, 0, 0, 0), 0.5f * (high - low), kBroadphaseColor);
        }
    }

    if (options.contacts) {
        for (const PhysicsWorld::DebugContact& c : world.Physics().LastStepContacts()) {
            out.Cross(c.point, 0.06f, kContactColor);
            out.Arrow(c.point, c.point + c.normal * 0.4f, kContactColor, 0.08f);
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
        for (const RuntimeWorld::GravityRegion& region : world.GravityRegions()) {
            DrawGravityRegion(out, region.component, region.position, region.rotation);
        }
    }

    if (options.frameAxes) {
        out.Axes(glm::vec3(0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), 2.0f);
        for (const RuntimeWorld::StaticBody& sb : world.StaticBodies()) out.Axes(sb.position, sb.rotation, 0.6f);
        for (const DynamicBody& body : bodies) {
            if (body.IsLive()) out.Axes(body.GetPresentedPosition(alpha), body.GetPresentedOrientation(alpha), 0.6f);
        }

    }

    if (options.lights) {
        for (const DynamicLight& light : BuildWorldLights(world, alpha)) DrawLight(out, light);
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
        }

        if (options.gravity && o.gravity) DrawGravityRegion(out, *o.gravity, position, rotation);
        if (options.frameAxes && (o.body || o.render || o.light )) out.Axes(position, rotation, 0.6f);
        if (options.lights && o.light) {
            DynamicLight light;
            light.kind = o.light->kind == SceneLightKind::Spot ? LightKind::Spot : LightKind::Point;
            light.position = position;
            light.direction = rotation * glm::vec3(0.0f, 0.0f, -1.0f);
            light.range = o.light->range;
            light.outerConeDegrees = o.light->outerConeDegrees;
            DrawLight(out, light);
        }


        if (options.atmosphere && o.atmosphere) {
            out.Sphere(position, o.atmosphere->referenceRadius, kAtmosphereColor, 48);
            out.Sphere(position, o.atmosphere->topRadius, kAtmosphereColor * 0.6f, 48);
        }
        if (options.waterBounds && o.fluidVolume) {
            const SceneFluidVolumeComponent& f = *o.fluidVolume;
            const auto scale=o.transform.scale;
            out.Box(position + rotation * (f.LocalCenter()*scale), rotation, f.HalfExtents()*scale, kFluidColor);
        }
    }
}
