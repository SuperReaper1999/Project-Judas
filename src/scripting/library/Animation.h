#pragma once

namespace ScriptBindings::LibraryParts {
inline constexpr char Animation[] = R"JS(
export class Ragdoll {
    constructor(id) {
        this.id = id;
    }
    get active() {
        return call("ragdollActive", this.id);
    }
    enter() {
        return call("ragdollEnter", this.id);
    }
    leave(seconds = 0.4) {
        return call("ragdollLeave", this.id, seconds);
    }
    set enabled(value) {
        call("ragdollEnabled", this.id, value);
    }
    get receiveContactEvents() {
        return call("ragdollContactEvents", this.id);
    }
    set receiveContactEvents(value) {
        call("ragdollContactEvents", this.id, value);
    }
    body(joint) {
        return entity(call("ragdollBody", this.id, joint));
    }
    configurePhysical(settings) {
        return call("ragdollPhysicalConfigure", this.id, settings);
    }
    setMode(mode, options = {}) {
        return call("ragdollMode", this.id, mode, options);
    }
    get physicalState() {
        return call("ragdollPhysicalState", this.id);
    }
    get balanceDiagnostics() {
        return call("ragdollBalanceDiagnostics", this.id);
    }
    set balanceDiagnostics(enabled) {
        call("ragdollBalanceDiagnostics", this.id, enabled);
    }
    configureBalance(settings) {
        return call("ragdollBalanceConfigure", this.id, settings);
    }
    requestBalance(task = "stand", options = {}) {
        return call("ragdollBalanceRequest", this.id, task, options);
    }
    cancelBalance() {
        return call("ragdollBalanceCancel", this.id);
    }
    get balanceState() {
        const r = call("ragdollBalanceState", this.id);
        return {
            ...r,
            supports: r.supports.map(s => ({
                ...s,
                entity: entity(s.entityId)
            }))
        };
    }
}
export class Animation {
    constructor(entityId) {
        this.entityId = entityId;
    }
    get info() {
        return call("animationInfo", this.entityId);
    }
    get clips() {
        return this.info.clips;
    }
    get playing() {
        return this.info.playing;
    }
    get time() {
        return this.info.time;
    }
    get speed() {
        return this.info.speed;
    }
    set speed(speed) {
        call("animationSet", this.entityId, {
            speed
        });
    }
    get loop() {
        return this.info.loop;
    }
    set loop(loop) {
        call("animationSet", this.entityId, {
            loop
        });
    }
    play(clip = "") {
        return call("animationPlay", this.entityId, clip);
    }
    pause() {
        return call("animationPause", this.entityId);
    }
    resume() {
        return this.play();
    }
    stop() {
        return call("animationStop", this.entityId);
    }
    seek(time) {
        return call("animationSeek", this.entityId, time);
    }
    advance(seconds) {
        return call("animationAdvance", this.entityId, seconds);
    }
    get sample() {
        return call("animationSample", this.entityId);
    }
    rootMotion(clip, from, to, loop = false) {
        return call("animationRootMotion", this.entityId, clip, from, to, loop);
    }
    crossFade(clip, seconds = 0.3) {
        return call("animationFade", this.entityId, clip, seconds);
    }
    get layers() {
        return this.info.layers;
    }
    layer(id, settings) {
        return call("animationLayer", this.entityId, id, settings);
    }
    removeLayer(id) {
        return call("animationRemoveLayer", this.entityId, id);
    }
    jointTransform(key, space = "world", presented = false) {
        return call("animationJointPose", this.entityId, key, space, presented);
    }
    limb(id, settings) {
        return call("animationLimb", this.entityId, id, settings);
    }
    removeLimb(id) {
        return call("animationRemoveLimb", this.entityId, id);
    }
    configureIK(settings) {
        return call("animationIKConfigure", this.entityId, settings);
    }
    ikTargets(targets) {
        return call("animationIKTargets", this.entityId, targets);
    }
    get ikStatus() {
        return call("animationIKStatus", this.entityId);
    }
}
extendEntity({
    setSocket(target, joint, offset = {}) {
        return target == null ? call("socketClear", this.id) : call("socketSet", this.id, target.id, joint, offset);
    },
    clearSocket() {
        return call("socketClear", this.id);
    },
    get animation() {
        return call("animationExists", this.id) ? new Animation(this.id) : null;
    },
    get ragdoll() {
        return call("ragdollExists", this.id) ? new Ragdoll(this.id) : null;
    }
});
)JS";
} // namespace ScriptBindings::LibraryParts
