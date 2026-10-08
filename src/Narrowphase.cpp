#include "Narrowphase.h"
#include "CollisionAsset.h"
#include "CollisionGeometry.h"
#include <algorithm>
#include <array>
#include <cmath>
#include "RadialTerrain.h"
#include "ContactGeometryInternal.h"

int PrimitiveCount(const Shape& shape) {
    return shape.type == ShapeType::CompoundBoxes ? static_cast<int>(shape.boxes.size()) : 1;
}

PrimitivePose PrimitiveAt(const Shape& shape, const RigidBody& parent, int index,
                          const ContactPreparedOrientation* prepared) {
    if (shape.type != ShapeType::CompoundBoxes) return {shape, parent, parent.position, parent.orientation, -shape.pivotOffset};
    const CompoundBox& child = shape.boxes[static_cast<std::size_t>(index)];
    RigidBody childPose = parent;
    const glm::dmat3 rotation = prepared && prepared->Matches(parent.orientation)
        ? prepared->rotation : ContactRotation(parent.orientation);
    childPose.position = glm::vec3(glm::dvec3(parent.position) + rotation * glm::dvec3(child.localCenter-shape.pivotOffset));
    if(child.rotation!=glm::quat(1,0,0,0))childPose.orientation=glm::normalize(parent.orientation*child.rotation);
    Shape primitive=child.type==ShapeType::Sphere?Shape::Sphere(child.radius):child.type==ShapeType::ConvexHull?Shape::Cooked(child.asset,child.assetId):Shape::Box(child.halfExtents);
    return {primitive, childPose, parent.position, parent.orientation, child.localCenter-shape.pivotOffset,child.rotation,child.key};
}

TerrainSample SampleTerrainAtWorld(const RadialTerrain& terrain, const RigidBody& body,
const glm::vec3& worldPoint) {
    const glm::quat inverseRotation = glm::conjugate(glm::normalize(body.orientation));
    TerrainSample sample = terrain.Sample(inverseRotation * (worldPoint - body.position));
    sample.surfacePoint = body.position + body.orientation * sample.surfacePoint;
    sample.outwardNormal = glm::normalize(body.orientation * sample.outwardNormal);
    return sample;
}

namespace {
    // Normals follow Contact's shape-A convention. For terrain A the normal
    // points *into* the terrain, so resolving dynamic B pushes it outward.
    // Each box contributes its corners and face centres: a hill can contact the
    // middle of a broad face even while all four corners clear the surface.
    ContactManifold TerrainVsPrimitive(const Shape& terrainShape, const RigidBody& terrainBody,
    const Shape& otherShape, const RigidBody& otherBody, float margin) {
        ContactManifold manifold;
        if (!terrainShape.terrain) return manifold;
        const RadialTerrain& terrain = *terrainShape.terrain;
        float boundingRadius = 0.0f;
        if (otherShape.type == ShapeType::Sphere) boundingRadius = otherShape.radius;
        else if (otherShape.type == ShapeType::Box) boundingRadius = glm::length(otherShape.halfExtents);
        else if(otherShape.type==ShapeType::ConvexHull&&otherShape.asset)boundingRadius=float(glm::length(glm::max(glm::abs(otherShape.asset->minimum-glm::dvec3(otherShape.pivotOffset)),glm::abs(otherShape.asset->maximum-glm::dvec3(otherShape.pivotOffset)))));
        else return manifold;
        if (glm::length(otherBody.position - terrainBody.position) >
        terrain.BoundRadius() + boundingRadius + margin) return manifold;
        if (otherShape.type == ShapeType::Sphere) {
            const TerrainSample sample = SampleTerrainAtWorld(terrain, terrainBody, otherBody.position);
            if (sample.signedDistance < otherShape.radius + margin) {
                Contact contact;
                contact.hit = true;
                contact.point = sample.surfacePoint;
                contact.normal = -sample.outwardNormal;
                contact.penetration = otherShape.radius - sample.signedDistance;
                manifold.Add(contact);
            }

            return manifold;
        }

        std::vector<Contact> candidates(otherShape.type==ShapeType::ConvexHull?otherShape.asset->vertices.size()+otherShape.asset->polygons.size():14);
        int count = 0;
        const glm::vec3 h = otherShape.halfExtents;
        const auto tryPoint = [&](const glm::vec3& localPoint) {
            const glm::vec3 worldPoint = otherBody.position + otherBody.orientation * localPoint;
            const TerrainSample sample = SampleTerrainAtWorld(terrain, terrainBody, worldPoint);
            if (sample.signedDistance >= margin) return;
            Contact contact;
            contact.hit = true;
            contact.point = sample.surfacePoint;
            contact.normal = -sample.outwardNormal;
            contact.penetration = -sample.signedDistance;
            candidates[static_cast<std::size_t>(count++)] = contact;
        };

        if(otherShape.type==ShapeType::ConvexHull){
            for(auto p:otherShape.asset->vertices)tryPoint(glm::vec3(p)-otherShape.pivotOffset);
            for(auto& face:otherShape.asset->polygons){glm::dvec3 center(0);for(auto v:face.vertices)center+=otherShape.asset->vertices[v];tryPoint(glm::vec3(center/double(face.vertices.size()))-otherShape.pivotOffset);}
        }else {
        for (int x : {-1, 1})
        for (int y : {-1, 1})
        for (int z : {-1, 1})
        tryPoint(glm::vec3(x * h.x, y * h.y, z * h.z));
        for (int axis = 0; axis < 3; ++axis) {
            for (int sign : {-1, 1}) {
                glm::vec3 point(0.0f);
                point[axis] = sign * h[axis];
                tryPoint(point);
            }
        }

        }
        std::sort(candidates.begin(), candidates.begin() + count,
        [](const Contact& a, const Contact& b) {
            return a.penetration > b.penetration;
        });
        for (int i = 0; i < std::min(count, 4); ++i) manifold.Add(candidates[static_cast<std::size_t>(i)]);
        return manifold;
    }
}

// namespace
// Uniform manifold dispatcher: sphere-involving pairs always produce at
// most one contact point (wrapped in a 1-point manifold); box-vs-box uses
// the real multi-point manifold (see Contacts.h for why that one
// specifically needs more than one point).
ContactManifold ComputeContacts(const Shape& shapeA, const RigidBody& bodyA, const Shape& shapeB,
const RigidBody& bodyB, float margin) {
    ContactManifold manifold;
    if(shapeA.type==ShapeType::Capsule||shapeB.type==ShapeType::Capsule){
        const bool flip=shapeB.type==ShapeType::Capsule;const auto& capsule=flip?shapeB:shapeA;const auto& cb=flip?bodyB:bodyA;const auto& other=flip?shapeA:shapeB;const auto& ob=flip?bodyA:bodyB;
        const auto axis=cb.orientation*glm::vec3(0,capsule.halfHeight,0);CapsuleDistance d;
        if(other.type==ShapeType::Box)d=CapsuleDistanceToBox(cb.position-axis,cb.position+axis,capsule.radius,ob.position,ob.orientation,other.halfExtents);
        else if(other.type==ShapeType::Sphere)d=CapsuleDistanceToSphere(cb.position-axis,cb.position+axis,capsule.radius,ob.position,other.radius);
        else if(other.asset){const auto x=SegmentGeometry(glm::dvec3(cb.position-axis),glm::dvec3(cb.position+axis),capsule.radius,PrimitiveAt(other,ob,0));if(!x.valid)return manifold;d={float(x.gap),glm::vec3(x.normal),glm::vec3(x.point)};}
        else return manifold;
        if(d.distance<=margin){Contact c;c.hit=true;c.point=d.otherPoint;c.normal=flip?-d.normal:d.normal;c.penetration=-d.distance;manifold.Add(c);}return manifold;
    }
    if (shapeA.type == ShapeType::Terrain) {
        return TerrainVsPrimitive(shapeA, bodyA, shapeB, bodyB, margin);
    }

    if (shapeB.type == ShapeType::Terrain) {
        manifold = TerrainVsPrimitive(shapeB, bodyB, shapeA, bodyA, margin);
        for (int i = 0; i < manifold.count; ++i) manifold.points[i].normal = -manifold.points[i].normal;
        return manifold;
    }

    if(shapeA.asset||shapeB.asset)return CookedContacts(PrimitiveAt(shapeA,bodyA,0),PrimitiveAt(shapeB,bodyB,0),margin);
    return PrimitiveContacts(shapeA, {bodyA.position, bodyA.orientation, glm::vec3(0)},
    shapeB, {bodyB.position, bodyB.orientation, glm::vec3(0)}, margin);
}

ContactManifold ComputeContacts(const PrimitivePose& a, const PrimitivePose& b, float margin,
                                const ContactPreparedOrientation* preparedA,
                                const ContactPreparedOrientation* preparedB) {
    if (a.shape.type==ShapeType::Capsule||b.shape.type==ShapeType::Capsule||a.shape.type == ShapeType::Terrain || b.shape.type == ShapeType::Terrain)
    return ComputeContacts(a.shape, a.body, b.shape, b.body, margin);
    if(a.shape.asset||b.shape.asset)return CookedContacts(a,b,margin);
    const auto identity=[](glm::quat q){return q==glm::quat(1,0,0,0);};
    // Preserve the proven primitive/identity-child arithmetic exactly. An extra
    // float normalization here can perturb an otherwise unimpulsed island member.
    if(identity(a.childRotation)&&identity(b.childRotation))
        return PrimitiveContacts(a.shape,{a.parentPosition,a.parentOrientation,a.parentLocalCenter},
            b.shape,{b.parentPosition,b.parentOrientation,b.parentLocalCenter},margin,preparedA,preparedB);
    const glm::quat qa=identity(a.childRotation)?a.parentOrientation:glm::normalize(a.parentOrientation*a.childRotation);
    const glm::quat qb=identity(b.childRotation)?b.parentOrientation:glm::normalize(b.parentOrientation*b.childRotation);
    auto out=PrimitiveContacts(a.shape, {a.parentPosition, qa, glm::conjugate(a.childRotation)*a.parentLocalCenter},
        b.shape, {b.parentPosition, qb, glm::conjugate(b.childRotation)*b.parentLocalCenter}, margin,
        identity(a.childRotation)?preparedA:nullptr,identity(b.childRotation)?preparedB:nullptr);
    const auto ra=ContactRotation(a.childRotation),rb=ContactRotation(b.childRotation);
    for(int i=0;i<out.count;++i){auto& c=out.points[i];c.localAnchorA=ra*c.localAnchorA;c.localWitnessA=ra*c.localWitnessA;c.localAnchorB=rb*c.localAnchorB;c.localWitnessB=rb*c.localWitnessB;}
    return out;
}

namespace {
    using namespace contact_geometry;
    float LowerFloat(double x) {
        float f=static_cast<float>(x);
        return double(f)>x ? std::nextafter(f,-std::numeric_limits<float>::infinity()) : f;
    }

    float UpperFloat(double x) {
        float f=static_cast<float>(x);
        return double(f)<x ? std::nextafter(f,std::numeric_limits<float>::infinity()) : f;
    }

    Aabb Bound(const std::array<Iv,3>& lo,const std::array<Iv,3>& hi) {
        Aabb out;
        for (int k=0;k<3;++k){
            out.min[k]=LowerFloat(lo[k].lo);
            out.max[k]=UpperFloat(hi[k].hi);
        }

        return out;
    }

    Aabb BoxAabb(const glm::vec3& position, const glm::quat& orientation,
    const glm::dvec3& offset, const glm::dvec3& half) {
        Rotation<Iv> r(orientation);
        std::array<Iv,3> lo,hi;
        for (int k=0;k<3;++k){
            Iv center{double(position[k])},extent(0);
            for (int j=0;j<3;++j){
                const Iv entry=r.n[j][k]/r.d;
                center=center+entry*Iv(double(offset[j]));
                extent=extent+Abs(entry)*Iv(double(half[j]));
            }

            lo[k]=center-extent;
            hi[k]=center+extent;
        }

        return Bound(lo,hi);
    }

    Aabb SphereAabb(const glm::vec3& position,double radius) {
        std::array<Iv,3> lo,hi;
        for (int k=0;k<3;++k){
            lo[k]=Iv(double(position[k]))-Iv(radius);
            hi[k]=Iv(double(position[k]))+Iv(radius);
        }

        return Bound(lo,hi);
    }
}

// namespace
Aabb ShapeAabb(const Shape& shape, const glm::vec3& position, const glm::quat& orientation) {
    switch (shape.type){
        case ShapeType::Sphere:return SphereAabb(position,shape.radius);
        case ShapeType::Box:return BoxAabb(position,orientation,glm::vec3(0),shape.halfExtents);
        case ShapeType::CompoundBoxes:{
            Aabb bound{position,position};
            bool first=true;
            for (const auto& child:shape.boxes){
                Shape primitive=child.type==ShapeType::Sphere?Shape::Sphere(child.radius):child.type==ShapeType::ConvexHull?Shape::Cooked(child.asset,child.assetId):Shape::Box(child.halfExtents);
                auto local=ShapeAabb(primitive,glm::vec3(0),child.rotation);
                const auto b=BoxAabb(position,orientation,glm::dvec3(child.localCenter)-glm::dvec3(shape.pivotOffset)+(glm::dvec3(local.min)+glm::dvec3(local.max))*.5,(glm::dvec3(local.max)-glm::dvec3(local.min))*.5);
                bound=first?b:bound.Union(b);
                first=false;
            }

            return bound;
        }

        case ShapeType::ConvexHull:
        case ShapeType::TriangleMesh:{if(!shape.asset)throw std::invalid_argument("missing cooked collision asset");return BoxAabb(position,orientation,(shape.asset->minimum+shape.asset->maximum)*.5-glm::dvec3(shape.pivotOffset),(shape.asset->maximum-shape.asset->minimum)*.5);}
        case ShapeType::Terrain:return SphereAabb(position,shape.terrain?shape.terrain->BoundRadius():0.0);
        case ShapeType::Capsule:return SphereAabb(position,double(shape.radius)+double(shape.halfHeight));
    }

    return {position,position};
}

PreparedShapeBounds PrepareShapeBounds(const Shape& shape,
                                      const ContactPreparedOrientation& orientation) {
    PreparedShapeBounds prepared;
    PrepareShapeBounds(shape,orientation,prepared);
    return prepared;
}

void PrepareShapeBounds(const Shape& shape, const ContactPreparedOrientation& orientation,
                        PreparedShapeBounds& prepared) {
    prepared.type=shape.type;
    prepared.radialRadius=0.0;
    prepared.box={};
    prepared.children.clear();
    const auto prepareBox=[&](const glm::vec3& offset,const glm::vec3& half) {
        PreparedBoxBound box;
        for (int k=0;k<3;++k) {
            Iv extent(0);
            for (int j=0;j<3;++j) {
                const Iv entry=orientation.normalizedInterval[j][k];
                box.offsetTerms[k][j]=entry*Iv(double(offset[j]));
                extent=extent+Abs(entry)*Iv(double(half[j]));
            }
            box.extent[k]=extent;
        }
        return box;
    };
    switch (shape.type) {
        case ShapeType::Box:
            prepared.box=prepareBox(glm::vec3(0),shape.halfExtents);
            break;
        case ShapeType::CompoundBoxes:
            prepared.children.reserve(shape.boxes.size());
            for (const auto& child:shape.boxes){Shape primitive=child.type==ShapeType::Sphere?Shape::Sphere(child.radius):child.type==ShapeType::ConvexHull?Shape::Cooked(child.asset,child.assetId):Shape::Box(child.halfExtents);auto local=ShapeAabb(primitive,glm::vec3(0),child.rotation);prepared.children.push_back(prepareBox(glm::dvec3(child.localCenter)-glm::dvec3(shape.pivotOffset)+(glm::dvec3(local.min)+glm::dvec3(local.max))*.5,(glm::dvec3(local.max)-glm::dvec3(local.min))*.5));}
            break;
        case ShapeType::ConvexHull:
        case ShapeType::TriangleMesh:prepared.box=prepareBox((shape.asset->minimum+shape.asset->maximum)*.5-glm::dvec3(shape.pivotOffset),(shape.asset->maximum-shape.asset->minimum)*.5);break;
        case ShapeType::Sphere: prepared.radialRadius=shape.radius; break;
        case ShapeType::Terrain: prepared.radialRadius=shape.terrain?shape.terrain->BoundRadius():0.0; break;
        case ShapeType::Capsule: prepared.radialRadius=double(shape.radius)+double(shape.halfHeight); break;
    }
}

Aabb ShapeAabb(const PreparedShapeBounds& prepared,const glm::vec3& position) {
    const auto translatedBox=[&](const PreparedBoxBound& box) {
        std::array<Iv,3> lo,hi;
        for (int k=0;k<3;++k) {
            Iv center{double(position[k])};
            for (int j=0;j<3;++j) center=center+box.offsetTerms[k][j];
            lo[k]=center-box.extent[k];
            hi[k]=center+box.extent[k];
        }
        return Bound(lo,hi);
    };
    switch (prepared.type) {
        case ShapeType::ConvexHull:
        case ShapeType::TriangleMesh:
        case ShapeType::Box: return translatedBox(prepared.box);
        case ShapeType::CompoundBoxes: {
            Aabb bound{position,position};
            bool first=true;
            for (const auto& child:prepared.children) {
                const Aabb next=translatedBox(child);
                bound=first?next:bound.Union(next);
                first=false;
            }
            return bound;
        }
        case ShapeType::Sphere:
        case ShapeType::Terrain:
        case ShapeType::Capsule: return SphereAabb(position,prepared.radialRadius);
    }
    return {position,position};
}

float ShapeBoundingRadius(const Shape& shape) {
    const auto length = [](const glm::vec3& v) {
        const auto iv = Vector<Iv>(v);
        return Sqrt(Dot(iv,iv));
    };

    switch (shape.type) {
        case ShapeType::Sphere: return shape.radius;
        case ShapeType::Box: return UpperFloat(length(shape.halfExtents).hi);
        case ShapeType::CompoundBoxes: {
            double r = 0.0;
            for (const CompoundBox& child : shape.boxes) {
                Shape primitive=child.type==ShapeType::Sphere?Shape::Sphere(child.radius):child.type==ShapeType::ConvexHull?Shape::Cooked(child.asset,child.assetId):Shape::Box(child.halfExtents);
                r = std::max(r, length(child.localCenter-shape.pivotOffset).hi + ShapeBoundingRadius(primitive));
            }

            return UpperFloat(r);
        }

        case ShapeType::ConvexHull:
        case ShapeType::TriangleMesh:{double r=0;for(auto p:shape.asset->vertices)r=std::max(r,glm::length(p-glm::dvec3(shape.pivotOffset)));return UpperFloat(r);}
        case ShapeType::Terrain: return shape.terrain ? shape.terrain->BoundRadius() : 0.0f;
        case ShapeType::Capsule: return UpperFloat((Iv(double(shape.radius)) + Iv(double(shape.halfHeight))).hi);
    }

    return 0.0f;
}
