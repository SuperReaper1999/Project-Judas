#pragma once

namespace ScriptBindings::LibraryParts {
inline constexpr char Physics[] = R"JS(
const castResult = hit => hit ? {
    ...hit,
    entity: entity(hit.entityId)
} : null;
const cast = (origin, direction, maximum, filter, shape) => castResult(call("cast", origin, direction, filter, {
    ...shape,
    maximum
}));
const castMany = (kind, requests, filter) => call("castMany", kind, requests, filter).map(castResult);
export class EntityGravity {
    constructor(id) {
        this.id = id;
    }
    get state() {
        const v = call("entityGravityState", this.id);
        return {
            ...v,
            source: entity(v.sourceId)
        };
    }
    get acceleration() {
        return this.state.acceleration;
    }
    select(source) {
        return call("entityGravitySelect", this.id, source?.id ?? "0");
    }
    setUniform(acceleration) {
        return call("entityGravityUniform", this.id, acceleration);
    }
    clear() {
        return call("entityGravityClear", this.id);
    }
}
export class Joint {
    constructor(id) {
        this.id = String(id);
    }
    get valid() {
        return call("jointValid", this.id);
    }
    get state() {
        return call("jointState", this.id);
    }
    configure(settings) {
        return call("jointConfigure", this.id, settings);
    }
    destroy() {
        return call("jointDestroy", this.id);
    }
    setEnabled(enabled) {
        return call("jointSet", this.id, {
            enabled
        });
    }
    setLimits(lower, upper, limits = true) {
        return call("jointSet", this.id, {
            lower,
            upper,
            limits
        });
    }
    setMotor(speed, maxForce, motor = true) {
        return call("jointSet", this.id, {
            speed,
            maxForce,
            motor
        });
    }
    setSpring(rest, stiffness, damping, spring = true) {
        return call("jointSet", this.id, {
            rest,
            stiffness,
            damping,
            spring
        });
    }
}
export const physics = {
    overlapShape: (shape, pose, limit = 32, filter = {}) => {
        const result = call("overlapShape", shape, pose, limit, filter);
        return {
            ...result,
            hits: result.hits.map(hit => ({
                ...hit,
                entity: entity(hit.entityId)
            }))
        };
    },
    cast2D: (shape, pose, direction, maximum, filter = {}) => castResult(call("cast2D", shape, pose, direction, maximum, filter)),
    gravity: point => call("gravitySample", point),
    createJoint: (owner, settings) => new Joint(call("jointCreate", owner.id, {
        ...settings,
        bodyAId: settings.bodyA?.id,
        bodyBId: settings.bodyB?.id ?? "0"
    })),
    closestPoint: (point, maximum, filter = {}) => {
        const hit = call("closestPoint", point, {
            x: 0,
            y: 0,
            z: 0
        }, filter, {
            maximum
        });
        return hit ? {
            ...hit,
            entity: entity(hit.entityId)
        } : null;
    },
    joint: owner => {
        const id = call("joint", owner.id);
        return id ? new Joint(id) : null;
    },
    raycast: (origin, direction, maximum, filter = {}) => cast(origin, direction, maximum, filter, {
        kind: "ray"
    }),
    raycastMany: (rays, filter = {}) => castMany("ray", rays, filter),
    sphereCast: (origin, radius, direction, maximum, filter = {}) => cast(origin, direction, maximum, filter, {
        kind: "sphere",
        radius
    }),
    sphereCastMany: (casts, filter = {}) => castMany("sphere", casts, filter),
    capsuleCast: (pose, radius, halfHeight, direction, maximum, filter = {}) => cast(pose.position, direction, maximum, filter, {
        kind: "capsule",
        rotation: pose.rotation,
        radius,
        halfHeight
    }),
    capsuleCastMany: (casts, filter = {}) => castMany("capsule", casts, filter),
    boxCast: (pose, halfExtents, direction, maximum, filter = {}) => cast(pose.position, direction, maximum, filter, {
        kind: "box",
        rotation: pose.rotation,
        halfExtents
    })
};
extendEntity({
    setColliderEnabled(enabled) {
        return call("colliderEnabled", this.id, enabled);
    },
    get collider() {
        return call("colliderInfo", this.id);
    },
    get body2D() {
        return call("body2DInfo", this.id);
    },
    get sleeping() {
        return call("sleeping", this.id);
    },
    get physicalMaterial() {
        return call("physicalMaterialInfo", this.id);
    },
    setPhysicalMaterial(asset = null, parameters = {}) {
        return call("physicalMaterialSet", this.id, asset, parameters);
    },
    setCollisionFilter(settings) {
        return call("collisionFilter", this.id, settings);
    },
    get gravity() {
        return new EntityGravity(this.id);
    },
    applyForce(value) {
        call("force", this.id, value);
    },
    applyImpulse(value) {
        call("impulse", this.id, value);
    },
    applyImpulseAtPoint(impulse, point) {
        call("impulseAtPoint", this.id, impulse, point);
    },
    applyTorque(value) {
        call("torque", this.id, value);
    },
    get mass() {
        return call("mass", this.id);
    },
    get inertiaWorld() {
        return call("inertiaWorld", this.id);
    },
    get centerOfMass() {
        return call("centerOfMass", this.id);
    },
    applyForceAtPoint(force, point) {
        call("forceAtPoint", this.id, force, point);
    },
    get velocity() {
        return call("velocity", this.id);
    },
    set velocity(value) {
        call("setVelocity", this.id, value);
    },
    get angularVelocity() {
        return call("angularVelocity", this.id);
    },
    set angularVelocity(value) {
        call("setAngularVelocity", this.id, value);
    },
    get motionType() {
        return call("motionType", this.id);
    },
    setMotionType(type, options = {}) {
        return call("motionTypeSet", this.id, type, options);
    },
    moveKinematic(target, seconds = 0) {
        return call("kinematicTarget", this.id, target, seconds);
    },
    setKinematicVelocity(linear, angular = {
        x: 0,
        y: 0,
        z: 0
    }) {
        return call("kinematicVelocity", this.id, linear, angular);
    },
    stopKinematic() {
        return call("kinematicStop", this.id);
    },
    get kinematicMotion() {
        return call("kinematicMotion", this.id);
    },
    pointVelocity(point) {
        return call("pointVelocity", this.id, point);
    }
});
extendWorld({
    overlap: (min, max, filter = {}) => call("overlap", min, max, filter).map(entity),
    sweepCapsule: (from, displacement, rotation = {
        w: 1,
        x: 0,
        y: 0,
        z: 0
    }, filter = {}) => call("sweep", from, displacement, filter, rotation)
});
)JS";
} // namespace ScriptBindings::LibraryParts
