#include "../../AssetDatabase.h"
#include "../../ResourceManager.h"
#include "../../RuntimeWorld.h"
#include "../BindingContext.h"
#include "../BindingRegistry.h"
#include "../BindingValues.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <set>
#include <sstream>
#include <stdexcept>
#include <tuple>

#include "../../AudioSystem.h"

using namespace ScriptBindingValues;
namespace ScriptBindings {
namespace {
JSValue HandleAudio(BindingContext &context, const std::string &op, int argc, JSValueConst *argv) {
    auto *s = &context;
    auto *c = context.js;
    auto &world = *s->world;
    auto arg = [&](int i) { return i < argc ? argv[i] : JS_UNDEFINED; };

    if (op == "audioGroup" || op == "audioGroupSet" || op == "audioDiagnostics") {
        auto *service = world.Resources() ? world.Resources()->GetAudioSystem() : nullptr;
        if (!service)
            return JS_NULL;
        if (op == "audioDiagnostics") {
            auto d = service->Diagnostics();
            auto o = JS_NewObject(c);
            for (auto item : {std::pair<const char *, double>{"voices", double(d.voices)},
                              {"streams", double(d.streams)},
                              {"bufferedBytes", double(d.bufferedBytes)},
                              {"streamBytes", double(d.streamBytes)},
                              {"streamHighWater", double(d.streamHighWater)},
                              {"pendingRetirements", double(d.pendingRetirements)},
                              {"reverbProcessors", double(d.reverbProcessors)},
                              {"underruns", double(d.underruns)},
                              {"decodedFrames", double(d.decodedFrames)},
                              {"occlusionQueries", double(world.AudioQueryCount())},
                              {"maximumDetachMilliseconds", d.maximumDetachMilliseconds}})
                JS_SetPropertyStr(c, o, item.first, JS_NewFloat64(c, item.second));
            return o;
        }
        if (!JS_IsString(arg(1)))
            return JS_ThrowTypeError(c, "sound group name required");
        auto name = String(c, arg(1));
        AudioGroupSettings group;
        if (!service->GetGroup(name, group))
            return op == "audioGroup" ? JS_NULL
                                      : JS_ThrowTypeError(c, "unknown authored sound group");
        if (op == "audioGroupSet") {
            if (!JS_IsObject(arg(2)))
                return JS_ThrowTypeError(c, "group settings required");
            auto gain = JS_GetPropertyStr(c, arg(2), "gain");
            bool supplied = !JS_IsUndefined(gain);
            JS_FreeValue(c, gain);
            if (supplied && !Number(c, arg(2), "gain", group.gain))
                return JS_ThrowTypeError(c, "finite group gain required");
            for (auto item : {std::pair<const char *, bool *>{"mute", &group.mute},
                              {"paused", &group.paused}}) {
                auto v = JS_GetPropertyStr(c, arg(2), item.first);
                if (!JS_IsUndefined(v)) {
                    if (!JS_IsBool(v)) {
                        JS_FreeValue(c, v);
                        return JS_ThrowTypeError(c, "boolean group setting required");
                    }
                    *item.second = JS_ToBool(c, v);
                }
                JS_FreeValue(c, v);
            }
            double fade;
            if (JS_ToFloat64(c, &fade, arg(3)) != 0 || !std::isfinite(fade) ||
                !service->SetGroup(name, group, float(fade)))
                return JS_ThrowTypeError(c, "gain 0..1, fade 0..60 seconds required");
            return JS_TRUE;
        }
        auto o = JS_NewObject(c);
        JS_SetPropertyStr(c, o, "gain", JS_NewFloat64(c, group.gain));
        JS_SetPropertyStr(c, o, "mute", JS_NewBool(c, group.mute));
        JS_SetPropertyStr(c, o, "paused", JS_NewBool(c, group.paused));
        return o;
    }

    EntityId id = 0;
    try {
        auto text = String(c, arg(1));
        size_t end;
        id = std::stoull(text, &end);
        if (end != text.size())
            id = 0;
    } catch (...) {
        id = 0;
    }
    const auto *definition = world.RuntimeDefinition(id);
    if (!definition)
        return JS_ThrowReferenceError(c, "stale or invalid entity %llu", (unsigned long long)id);

    if (op == "audioEnabled" && !JS_IsBool(arg(2)))
        return JS_ThrowTypeError(c, "audio enabled must be boolean");

    if (op == "audioEnabled")
        return JS_NewBool(c, world.SetAudioEnabled(id, JS_ToBool(c, arg(2))));

    if (op == "audioInfo") {
        for (const auto &emitter : world.AudioEmitters())
            if (emitter.id == id) {
                auto o = JS_NewObject(c);
                AudioVoiceSnapshot snapshot;
                auto *service = world.Resources() ? world.Resources()->GetAudioSystem() : nullptr;
                bool loaded = service && service->Snapshot(emitter.voice, snapshot);
                const char *state = !emitter.error.empty() && emitter.error != "loading" ? "failed"
                                    : !loaded                                            ? "loading"
                                    : !snapshot.error.empty()                            ? "failed"
                                    : snapshot.seeking && !snapshot.ready                ? "seeking"
                                    : !snapshot.ready                                    ? "loading"
                                    : snapshot.state == AudioPlaybackState::Paused       ? "paused"
                                    : snapshot.state == AudioPlaybackState::Finished     ? "ended"
                                    : snapshot.state == AudioPlaybackState::Playing
                                        ? (snapshot.starved ? "starved" : "playing")
                                        : "ready";
                JS_SetPropertyStr(c, o, "state", JS_NewString(c, state));
                JS_SetPropertyStr(c, o, "error",
                                  emitter.error.empty() ? JS_NULL
                                                        : JS_NewString(c, emitter.error.c_str()));
                for (auto item :
                     {std::pair<const char *, bool>{"enabled", emitter.settings.enabled},
                      {"playing", loaded && snapshot.state == AudioPlaybackState::Playing},
                      {"requested", emitter.wantPlay},
                      {"ready", loaded && snapshot.ready},
                      {"streamed", emitter.settings.loading == AudioLoading::Streamed},
                      {"loop", emitter.settings.loop},
                      {"starved", snapshot.starved}})
                    JS_SetPropertyStr(c, o, item.first, JS_NewBool(c, item.second));
                JS_SetPropertyStr(c, o, "position",
                                  loaded && snapshot.cursorAvailable
                                      ? JS_NewFloat64(c, snapshot.positionSeconds)
                                      : JS_NULL);
                JS_SetPropertyStr(c, o, "duration",
                                  loaded && snapshot.durationKnown
                                      ? JS_NewFloat64(c, snapshot.durationSeconds)
                                      : JS_NULL);
                for (auto item :
                     {std::pair<const char *, double>{"bufferBytes", double(snapshot.bufferBytes)},
                      {"underruns", double(snapshot.underruns)},
                      {"dopplerRatio", snapshot.dopplerRatio},
                      {"occlusionGain", snapshot.occlusionGain},
                      {"cutoff", snapshot.cutoff},
                      {"distanceGain", snapshot.distanceGain}})
                    JS_SetPropertyStr(c, o, item.first, JS_NewFloat64(c, item.second));
                return o;
            }
        return JS_NULL;
    }

    if (op == "audioSeek") {
        double seconds;
        if (JS_ToFloat64(c, &seconds, arg(2)) != 0 || !std::isfinite(seconds) || seconds < 0 ||
            seconds > 1e8)
            return JS_ThrowTypeError(c, "finite seek seconds required");
        return JS_NewBool(c, world.SeekAudio(id, seconds));
    }

    if (op == "audioVelocity") {
        glm::vec3 velocity;
        if (!JS_IsNull(arg(2)) && !ReadVec(c, arg(2), velocity))
            return JS_ThrowTypeError(c, "world velocity or null required");
        return JS_NewBool(c, world.SetAudioVelocity(id, JS_IsNull(arg(2))
                                                            ? std::nullopt
                                                            : std::optional<glm::vec3>(velocity)));
    }

    if (op == "audioOneShot") {
        std::optional<std::string> clip;
        if (!JS_IsUndefined(arg(2))) {
            if (!JS_IsString(arg(2)))
                return JS_ThrowTypeError(c, "registered audio asset ID required");
            clip = String(c, arg(2));
            if (clip->empty())
                return JS_ThrowTypeError(c, "registered audio asset ID required");
        }
        std::string error;
        if (!world.PlayAudioOneShot(id, error, clip ? &*clip : nullptr)) {
            if (error.empty())
                return JS_FALSE;
            return JS_ThrowTypeError(c, "one-shot: %s", error.c_str());
        }
        return JS_TRUE;
    }

    if (op == "audioSettings") {
        if (!JS_IsObject(arg(2)))
            return JS_ThrowTypeError(c, "audio settings required");
        for (const auto &emitter : world.AudioEmitters())
            if (emitter.id == id) {
                AudioSettings settings = emitter.settings;
                auto options = arg(2);
                if (!KnownOptions(c, options,
                                  {"volume", "pitch", "doppler", "send", "occludedGain",
                                   "occludedCutoff", "referenceDistance", "maximumDistance",
                                   "rolloff", "loop", "spatial", "occlusion", "bypass", "group",
                                   "loading", "attenuation", "streamPageFrames", "occlusionLayers"}))
                    return JS_ThrowTypeError(c, "unknown audio setting");
                for (auto item : {std::pair<const char *, float *>{"volume", &settings.volume},
                                  {"pitch", &settings.pitch},
                                  {"doppler", &settings.doppler},
                                  {"send", &settings.send},
                                  {"occludedGain", &settings.occludedGain},
                                  {"occludedCutoff", &settings.occludedCutoff},
                                  {"referenceDistance", &settings.referenceDistance},
                                  {"maximumDistance", &settings.maximumDistance},
                                  {"rolloff", &settings.rolloff}}) {
                    auto v = JS_GetPropertyStr(c, options, item.first);
                    bool present = !JS_IsUndefined(v);
                    JS_FreeValue(c, v);
                    if (present && !Number(c, options, item.first, *item.second))
                        return JS_ThrowTypeError(c, "finite audio setting required");
                }
                for (auto item : {std::pair<const char *, bool *>{"loop", &settings.loop},
                                  {"spatial", &settings.spatial},
                                  {"occlusion", &settings.occlusion},
                                  {"bypass", &settings.bypass}}) {
                    auto v = JS_GetPropertyStr(c, options, item.first);
                    if (!JS_IsUndefined(v)) {
                        if (!JS_IsBool(v)) {
                            JS_FreeValue(c, v);
                            return JS_ThrowTypeError(c, "boolean audio setting required");
                        }
                        *item.second = JS_ToBool(c, v);
                    }
                    JS_FreeValue(c, v);
                }
                for (auto field : {"group", "loading", "attenuation"}) {
                    auto v = JS_GetPropertyStr(c, options, field);
                    if (!JS_IsUndefined(v)) {
                        if (!JS_IsString(v)) {
                            JS_FreeValue(c, v);
                            return JS_ThrowTypeError(c, "string audio setting required");
                        }
                        auto value = String(c, v);
                        if (std::string(field) == "group")
                            settings.group = value;
                        else if (std::string(field) == "loading") {
                            if (value != "buffered" && value != "streamed") {
                                JS_FreeValue(c, v);
                                return JS_ThrowTypeError(c, "buffered/streamed required");
                            }
                            settings.loading = value == "streamed" ? AudioLoading::Streamed
                                                                   : AudioLoading::Buffered;
                        } else {
                            if (value != "none" && value != "inverse" && value != "linear") {
                                JS_FreeValue(c, v);
                                return JS_ThrowTypeError(c, "none/inverse/linear required");
                            }
                            settings.attenuation = value == "none"      ? AudioAttenuation::None
                                                   : value == "inverse" ? AudioAttenuation::Inverse
                                                                        : AudioAttenuation::Linear;
                        }
                    }
                    JS_FreeValue(c, v);
                }
                auto frames = JS_GetPropertyStr(c, options, "streamPageFrames");
                if (!JS_IsUndefined(frames)) {
                    double n;
                    if (JS_ToFloat64(c, &n, frames) != 0 || !std::isfinite(n) ||
                        n != std::floor(n) || n < 1024 || n > 16384) {
                        JS_FreeValue(c, frames);
                        return JS_ThrowTypeError(c,
                                                 "integer stream page frames 1024..16384 required");
                    }
                    settings.streamPageFrames = unsigned(n);
                }
                JS_FreeValue(c, frames);
                auto mask = JS_GetPropertyStr(c, options, "occlusionLayers");
                if (!JS_IsUndefined(mask)) {
                    if (!JS_IsArray(mask)) {
                        JS_FreeValue(c, mask);
                        return JS_ThrowTypeError(c, "collision layer names required");
                    }
                    auto len = JS_GetPropertyStr(c, mask, "length");
                    uint32_t n;
                    JS_ToUint32(c, &n, len);
                    JS_FreeValue(c, len);
                    if (n > 64) {
                        JS_FreeValue(c, mask);
                        return JS_ThrowTypeError(c, "at most 64 layers");
                    }
                    settings.occlusionMask = 0;
                    for (uint32_t i = 0; i < n; ++i) {
                        auto v = JS_GetPropertyUint32(c, mask, i);
                        int layer =
                            JS_IsString(v) ? world.Categories().collision.Find(String(c, v)) : -1;
                        JS_FreeValue(c, v);
                        if (layer < 0) {
                            JS_FreeValue(c, mask);
                            return JS_ThrowTypeError(c, "unknown collision layer");
                        }
                        settings.occlusionMask |= CategoryMask(1) << layer;
                    }
                }
                JS_FreeValue(c, mask);
                auto *service = world.Resources() ? world.Resources()->GetAudioSystem() : nullptr;
                AudioGroupSettings group;
                if (!ValidAudioSettings(settings) ||
                    (service && !service->GetGroup(settings.group, group)))
                    return JS_ThrowTypeError(c, "invalid audio settings or unknown group");
                return JS_NewBool(c, world.SetAudioSettings(id, settings));
            }
        return JS_FALSE;
    }

    if (op == "playAudio" || op == "stopAudio" || op == "pauseAudio" || op == "resumeAudio")
        return JS_NewBool(c, op == "playAudio"    ? world.PlayAudio(id)
                             : op == "stopAudio"  ? world.StopAudio(id)
                             : op == "pauseAudio" ? world.PauseAudio(id)
                                                  : world.ResumeAudio(id));
    return JS_ThrowTypeError(c, "unhandled Audio operation");
}
} // namespace
void RegisterAudioBindings(BindingRegistry &registry) {
    registry.Add("Audio",
                 {"audioGroup", "audioGroupSet", "audioDiagnostics", "audioEnabled", "audioInfo",
                  "audioSeek", "audioVelocity", "audioOneShot", "audioSettings", "playAudio",
                  "stopAudio", "pauseAudio", "resumeAudio"},
                 HandleAudio);
}
} // namespace ScriptBindings
