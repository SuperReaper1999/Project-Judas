#pragma once

namespace ScriptBindings::LibraryParts {
inline constexpr char Audio[] = R"JS(
export const audio = {
    group: name => call("audioGroup", name),
    setGroup: (name, settings, fadeSeconds = 0) => call("audioGroupSet", name, settings, fadeSeconds),
    get diagnostics() {
        return call("audioDiagnostics");
    }
};
extendEntity({
    get audio() {
        return call("audioInfo", this.id);
    },
    setAudio(settings) {
        return call("audioSettings", this.id, settings);
    },
    seekAudio(seconds) {
        return call("audioSeek", this.id, seconds);
    },
    setAudioVelocity(velocity = null) {
        return call("audioVelocity", this.id, velocity);
    },
    playAudioOneShot(clip = undefined) {
        return call("audioOneShot", this.id, clip);
    },
    setAudioEnabled(enabled) {
        return call("audioEnabled", this.id, enabled);
    },
    playAudio() {
        return call("playAudio", this.id);
    },
    stopAudio() {
        return call("stopAudio", this.id);
    },
    pauseAudio() {
        return call("pauseAudio", this.id);
    },
    resumeAudio() {
        return call("resumeAudio", this.id);
    }
});
)JS";
} // namespace ScriptBindings::LibraryParts
