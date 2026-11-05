#include "NamedAuthoring.h"
#include "Prefab.h"
#include "SceneFingerprint.h"
#include "SceneSerialization.h"
#include "editor/EditorDocument.h"

#include <cstdio>
#include <functional>
#include <limits>
#include <vector>

namespace {
int checks = 0, failures = 0;
void Check(bool okay, const char *message) {
    ++checks;
    failures += !okay;
    std::printf("%s %s\n", okay ? "PASS" : "FAIL", message);
}
} // namespace

int main() {
    Scene legacy;
    const auto id = legacy.CreateObject("Portable smoke").id;
    legacy.Find(id)->particleEmitter = ParticleEmitterSettings{};
    std::string text, error, fingerprint;
    Check(SaveSceneToString(legacy, text) &&
              ComputeSceneFingerprint(legacy, fingerprint, error),
          "legacy emitter remains serializable and fingerprintable");
    Check(text.find("particle.backend") == std::string::npos &&
              text.find("particle.collision") == std::string::npos &&
              text.find("particle.impacts") == std::string::npos,
          "default emitters add no V2 canonical properties");
    Scene round;
    Check(LoadSceneFromString(text, round, error) && ScenesEqual(legacy, round) &&
              round.Find(id)->particleEmitter->backend == ParticleBackend::Cpu &&
              !round.Find(id)->particleEmitter->collision &&
              round.Find(id)->particleEmitter->impacts == ParticleImpactDelivery::Off,
          "old scene input retains portable CPU visual-only defaults");
    std::string roundText, roundFingerprint;
    Check(SaveSceneToString(round, roundText) && roundText == text &&
              ComputeSceneFingerprint(round, roundFingerprint, error) &&
              roundFingerprint == fingerprint,
          "default scene preserves canonical text and fingerprint on roundtrip");

    auto authored = legacy;
    auto &settings = *authored.Find(id)->particleEmitter;
    settings.backend = ParticleBackend::Auto;
    settings.collision = settings.useWind = true;
    settings.collisionRadius = .125f;
    settings.restitution = .7f;
    settings.friction = .6f;
    settings.response = ParticleResponse::Stick;
    settings.collisionLayers = 0x8000000000000001ull;
    settings.maxContacts = 13;
    settings.impacts = ParticleImpactDelivery::Reliable;
    settings.impactCellSize = .25f;
    settings.impactWeight = 2.5f;
    settings.maxImpactCells = 37;
    Check(ValidParticleSettings(settings), "coherent collision and event authoring passes validation");
    Check(SaveSceneToString(authored, roundText) &&
              roundText.find("9223372036854775809") != std::string::npos &&
              LoadSceneFromString(roundText, round, error) && ScenesEqual(authored, round),
          "every V2 property and high-bit collision mask roundtrips exactly");
    std::string named, decoded;
    Check(LegacyToNamed(roundText, "scene", named, error) &&
              NamedToLegacy(named, "scene", decoded, error) &&
              LoadSceneFromString(decoded, round, error) && ScenesEqual(authored, round),
          "named authoring codec retains collision and event properties");
    Scene prefab;
    Check(CreatePrefab(authored, id, prefab, error) && ValidatePrefab(prefab, error) &&
              prefab.Objects().size() == 1 &&
              ParticleSettingsEqual(*prefab.Objects()[0].particleEmitter, settings),
          "ordinary prefab extraction retains the complete emitter definition");

    const std::vector<std::function<void(ParticleEmitterSettings &)>> variations = {
        [](auto &s) { s.backend = ParticleBackend::Auto; },
        [](auto &s) { s.collision = true; }, [](auto &s) { s.useWind = true; },
        [](auto &s) { s.collisionRadius = .1f; }, [](auto &s) { s.restitution = .9f; },
        [](auto &s) { s.friction = .8f; },
        [](auto &s) { s.response = ParticleResponse::Remove; },
        [](auto &s) { s.collisionLayers = 0x8000000000000001ull; },
        [](auto &s) { s.maxContacts = 3; },
        [](auto &s) { s.impacts = ParticleImpactDelivery::Cosmetic; },
        [](auto &s) { s.impactCellSize = .125f; }, [](auto &s) { s.impactWeight = 3; },
        [](auto &s) { s.maxImpactCells = 7; }};
    for (const auto &variation : variations) {
        auto changed = legacy;
        variation(*changed.Find(id)->particleEmitter);
        std::string digest;
        Check(!ParticleSettingsEqual(*changed.Find(id)->particleEmitter,
                                     *legacy.Find(id)->particleEmitter) &&
                  ComputeSceneFingerprint(changed, digest, error) && digest != fingerprint,
              "each V2 option affects equality and authored compatibility identity");
    }
    EditorDocument document;
    document.GetScene() = legacy;
    document.BeginEdit();
    *document.GetScene().Find(id)->particleEmitter = settings;
    document.CommitEdit();
    Check(ScenesEqual(document.GetScene(), authored), "document edit owns the complete new definition");
    document.Undo();
    Check(ScenesEqual(document.GetScene(), legacy), "undo restores all legacy defaults");
    document.Redo();
    Check(ScenesEqual(document.GetScene(), authored), "redo restores collision and impact authoring");

    const std::vector<std::function<void(ParticleEmitterSettings &)>> invalid = {
        [](auto &s) { s.backend = ParticleBackend(99); },
        [](auto &s) { s.response = ParticleResponse(99); },
        [](auto &s) { s.impacts = ParticleImpactDelivery(99); },
        [](auto &s) { s.collisionRadius = 0; }, [](auto &s) { s.restitution = 1.1f; },
        [](auto &s) { s.friction = -.1f; }, [](auto &s) { s.maxContacts = 0; },
        [](auto &s) { s.maxContacts = 33; }, [](auto &s) { s.impactCellSize = 0; },
        [](auto &s) { s.impactCellSize = 1e-7f; },
        [](auto &s) { s.impactWeight = std::numeric_limits<float>::infinity(); },
        [](auto &s) { s.maxImpactCells = 0; }, [](auto &s) { s.maxImpactCells = 65537; }};
    for (const auto &variation : invalid) {
        auto bad = settings;
        variation(bad);
        Check(!ValidParticleSettings(bad), "invalid enum or unsafe budget is rejected without coercion");
    }
    for (const auto &badLine : {"particle.backend \"unknown\"", "particle.response \"ghost\"",
                                "particle.impacts \"every-particle\"",
                                "particle.max-contacts -1", "particle.collision-layers -1"}) {
        auto bad = text;
        bad.insert(bad.rfind("end"), std::string(badLine) + '\n');
        const auto before = round;
        Check(!LoadSceneFromString(bad, round, error) && ScenesEqual(round, before),
              "malformed authored particle options fail transactionally");
    }
    std::printf("SUMMARY %d checks %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
