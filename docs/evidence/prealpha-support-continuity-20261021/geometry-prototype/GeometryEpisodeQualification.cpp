#include "PrimitiveContactEpisode.h"

#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <random>
#include <string>

// Source-only qualification fixture. No engine/world inputs or acceptance
// settings are retuned. Assertions remain active in Release/NDEBUG builds.
namespace {
using persistent_contact_geometry::Episode;
using persistent_contact_geometry::Options;
std::uint64_t checks = 0, failures = 0, observedQueries = 0;

void Require(bool condition, const std::string &message) {
    ++checks;
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

template <class T> bool SameBits(T a, T b) {
    return std::memcmp(&a, &b, sizeof(T)) == 0;
}
bool SameVector(glm::vec3 a, glm::vec3 b) {
    return SameBits(a.x, b.x) && SameBits(a.y, b.y) && SameBits(a.z, b.z);
}
bool SameVector(glm::dvec3 a, glm::dvec3 b) {
    return SameBits(a.x, b.x) && SameBits(a.y, b.y) && SameBits(a.z, b.z);
}
bool SameManifold(const ContactManifold &a, const ContactManifold &b) {
    if (a.count != b.count || a.uncertain != b.uncertain)
        return false;
    for (int i = 0; i < a.count; ++i) {
        const auto &x = a.points[i], &y = b.points[i];
        if (x.hit != y.hit || !SameVector(x.point, y.point) || !SameVector(x.normal, y.normal) ||
            !SameBits(x.penetration, y.penetration) ||
            !SameBits(x.signedSeparation, y.signedSeparation) ||
            !SameVector(x.preciseNormal, y.preciseNormal) ||
            !SameVector(x.localAnchorA, y.localAnchorA) ||
            !SameVector(x.localAnchorB, y.localAnchorB) ||
            !SameVector(x.localWitnessA, y.localWitnessA) ||
            !SameVector(x.localWitnessB, y.localWitnessB) ||
            x.hasLocalAnchors != y.hasLocalAnchors ||
            x.anchorAInWorldFrame != y.anchorAInWorldFrame ||
            x.anchorBInWorldFrame != y.anchorBInWorldFrame ||
            x.separationState != y.separationState)
            return false;
    }
    return true;
}

Options ShadowOptions() {
    Options options;
    options.shadowNative = true;
    return options;
}

ContactManifold Check(const Shape &a, const ContactPose &pa, const Shape &b,
                      const ContactPose &pb, Episode &episode, const std::string &label,
                      Options options = ShadowOptions(), float margin = 0) {
    const auto before = persistent_contact_geometry::GetStats();
    const auto actual = persistent_contact_geometry::Query(a, pa, b, pb, episode, options, margin);
    const auto native = PrimitiveContacts(a, pa, b, pb, margin);
    const auto after = persistent_contact_geometry::GetStats();
    ++observedQueries;
    Require(SameManifold(actual, native), label + ": independent native output");
    // Shadow mode deliberately returns the oracle output. This second check is
    // essential: comparing returned output alone could hide a broken proposal.
    Require(after.shadowMismatches == before.shadowMismatches,
            label + ": proposed shortcut matches native before shadow substitution");
    return actual;
}

ContactPose Pose(glm::vec3 position, glm::quat orientation = glm::quat(1, 0, 0, 0)) {
    return {position, orientation, glm::vec3(0)};
}

void FaceContinuationAndExits() {
    const auto floor = Shape::Box({4, .3f, 4});
    const auto box = Shape::Box({.5f, .5f, .5f});
    const auto pa = Pose({0, 0, 0});
    Episode episode;
    const auto first = Check(floor, pa, box, Pose({0, .75f, 0}), episode, "uncut seed");
    Require(first.count == 4 && !first.uncertain, "positive fixture has four native contacts");
    const auto seeded = persistent_contact_geometry::GetStats();
    for (unsigned i = 1; i <= 24; ++i) {
        const float at = float(i);
        const auto rotation = glm::angleAxis(.001f * at, glm::vec3(0, 1, 0)) *
                              glm::angleAxis(.0005f * at, glm::vec3(1, 0, 0));
        Check(floor, pa, box, Pose({.003f * at, .75f, -.002f * at}, rotation), episode,
              "changed-pose four-vertex continuation " + std::to_string(i));
    }
    Require(persistent_contact_geometry::GetStats().faceReplay > seeded.faceReplay,
            "positive face replay mechanism was actually exercised");
    const auto inside = Check(floor, pa, box, Pose({0, .75f, 0}), episode, "inside reseed");
    const auto outsideBefore = persistent_contact_geometry::GetStats();
    const auto clipped = Check(floor, pa, box, Pose({4.25f, .75f, 0}), episode,
                               "incident vertices cross finite reference edge");
    Require(clipped.count > 0 && !clipped.uncertain, "edge-exit control still physically contacts");
    Require(persistent_contact_geometry::GetStats().faceOutside > outsideBefore.faceOutside,
            "finite edge exit disables retained uncut patch");
    Require(!SameManifold(inside, clipped), "negative oracle detects stale old face anchors");

    Check(floor, pa, box, Pose({0, .75f, 0}), episode, "gap-exit reseed");
    const auto gapBefore = persistent_contact_geometry::GetStats();
    Check(floor, pa, box,
          Pose({0, .79f, 0}, glm::angleAxis(.08f, glm::vec3(0, 0, 1))), episode,
          "some incident vertices leave normal contact");
    Require(persistent_contact_geometry::GetStats().faceGapRejected > gapBefore.faceGapRejected,
            "partial normal separation falls back to native clipping");

    Check(floor, pa, box, Pose({0, .75f, 0}), episode, "axis-change reseed");
    const auto changedBefore = persistent_contact_geometry::GetStats();
    Check(floor, pa, box, Pose({4.45f, 0, 0}), episode, "response winner changes to side face");
    Require(persistent_contact_geometry::GetStats().faceChanged > changedBefore.faceChanged,
            "response winner/sign change invalidates the retained face identity");
}

void SeparationContactAndDegeneracy() {
    const auto box = Shape::Box({.5f, .5f, .5f});
    const auto pa = Pose({0, 0, 0});
    Episode episode;
    const auto empty = Check(box, pa, box, Pose({1.4f, 0, 0}), episode, "separating face seed");
    Require(empty.count == 0 && !empty.uncertain, "separation seed has native empty result");
    const auto hintBefore = persistent_contact_geometry::GetStats();
    Check(box, Pose({.015f, .003f, 0}), box, Pose({1.45f, .006f, 0}), episode,
          "fresh changed-pose separation proof");
    Require(persistent_contact_geometry::GetStats().certifiedSeparated > hintBefore.certifiedSeparated,
            "retained separating axis is actually revalidated successfully");
    const auto entered = Check(box, pa, box, Pose({.95f, 0, 0}), episode,
                               "old separating feature enters penetration");
    Require(entered.count > 0 && !entered.uncertain, "contact entry does not reuse an old miss");
    Require(!SameManifold(empty, entered), "negative oracle detects stale empty result");

    // Force a syntactically valid stale cross-axis hint that is now exactly
    // degenerate. It must not be accepted as a separating direction.
    episode.separatingAxis = 6;
    const auto uncertainBefore = persistent_contact_geometry::GetStats();
    Check(box, pa, box, Pose({.9f, .01f, 0}), episode, "cross axis becomes exactly parallel");
    Require(persistent_contact_geometry::GetStats().uncertifiedHints > uncertainBefore.uncertifiedHints,
            "degenerate cached direction takes native fallback");
    for (float angle : {0x1p-20f, -0x1p-20f, 0x1p-40f, 0.f}) {
        episode.separatingAxis = 6;
        Check(box, pa, box, Pose({.95f, .01f, 0}, glm::angleAxis(angle, glm::vec3(0, 1, 0))),
              episode, "nearly parallel cached direction");
    }
}

void ExactBoundaryAndNonunitOrientation() {
    const auto box = Shape::Box({.5f, .5f, .5f});
    Episode episode;
    const float inside = std::nextafter(1.f, 0.f);
    const float outside = std::nextafter(1.f, std::numeric_limits<float>::infinity());
    const auto a = Pose({0, 0, 0});
    const auto first = Check(box, a, box, Pose({inside, 0, 0}), episode, "one ULP inside");
    const auto exact = Check(box, a, box, Pose({1, 0, 0}), episode, "exact face contact");
    const auto last = Check(box, a, box, Pose({outside, 0, 0}), episode, "one ULP outside");
    Require(first.count > 0 && exact.count > 0 && last.count == 0,
            "strict one-ULP contact/separation acceptance preserved");
    Check(box, a, box, Pose({inside, 0, 0}), episode, "one ULP reentry after miss");

    for (float scale : {3.f, -3.f, .125f, 0x1p-50f, 0x1p50f}) {
        const auto qa = glm::quat(scale, 0, 0, 0);
        const auto qb = glm::angleAxis(.03125f, glm::vec3(0, 0, 1)) * scale;
        Check(box, Pose({0, 0, 0}, qa), box, Pose({.97f, 0, 0}, qb), episode,
              "nonunit or sign-reversed quaternion");
    }
    const auto zero = Check(box, a, box, Pose({.9f, 0, 0}, glm::quat(0, 0, 0, 0)), episode,
                             "invalid zero quaternion uncertainty");
    Require(zero.uncertain && !episode.initialized, "uncertainty clears geometric episode");
    const auto nan = std::numeric_limits<float>::quiet_NaN();
    const auto invalid = Check(box, a, box, Pose({.9f, 0, 0}, glm::quat(1, nan, 0, 0)), episode,
                                "invalid nonfinite quaternion uncertainty");
    Require(invalid.uncertain, "native invalid-input uncertainty is not converted to a miss");
}

void DeterministicMovingPairsAndCrossAxis() {
    std::mt19937 random(0x51c0ffeeu);
    std::uniform_real_distribution<float> signedUnit(-1.f, 1.f);
    const auto randomRotation = [&] {
        glm::vec3 axis(signedUnit(random), signedUnit(random), signedUnit(random));
        if (glm::length(axis) < .1f)
            axis = {1, 0, 0};
        return glm::angleAxis(2.5f * signedUnit(random), glm::normalize(axis));
    };
    const auto thin = Shape::Box({.8f, .12f, .14f});
    bool crossAxisRevalidated = false;
    for (unsigned i = 0; i < 4096 && !crossAxisRevalidated; ++i) {
        Episode episode;
        const auto pa = Pose({0, 0, 0}, randomRotation());
        const auto pb = Pose({signedUnit(random), signedUnit(random), signedUnit(random)},
                             randomRotation());
        const auto native = Check(thin, pa, thin, pb, episode, "cross-axis seed search");
        if (native.count || native.uncertain || episode.separatingAxis < 6)
            continue;
        const auto before = persistent_contact_geometry::GetStats();
        auto movedA = pa, movedB = pb;
        movedA.position += glm::vec3(.003f, -.007f, .002f);
        movedB.position += glm::vec3(.003f, -.007f, .002f);
        Check(thin, movedA, thin, movedB, episode, "changed-pose edge-axis certificate");
        crossAxisRevalidated = persistent_contact_geometry::GetStats().certifiedSeparated >
                               before.certifiedSeparated;
    }
    Require(crossAxisRevalidated, "positive cross-edge-axis revalidation was exercised");

    for (unsigned track = 0; track < 12; ++track) {
        Episode episode;
        const auto qa = randomRotation(), qb = randomRotation();
        const auto first = Shape::Box({.25f + .04f * track, .3f, .45f});
        const auto second = Shape::Box({.42f, .2f + .02f * track, .25f});
        for (unsigned sample = 0; sample < 160; ++sample) {
            const float t = .047f * sample;
            const auto rotatingA = glm::angleAxis(.017f * t, glm::vec3(0, 1, 0)) * qa;
            const auto rotatingB = glm::angleAxis(.19f * t, glm::vec3(1, 0, 0)) * qb;
            const glm::vec3 origin = track % 3 == 0 ? glm::vec3(100000, -200000, 150000)
                                                   : glm::vec3(0);
            Check(first, Pose(origin, rotatingA), second,
                  Pose(origin + glm::vec3(1.1f * std::cos(t), .5f * std::sin(.7f * t),
                                          .6f * std::sin(t)), rotatingB),
                  episode, "deterministic moving pair " + std::to_string(track) + "/" +
                               std::to_string(sample));
        }
    }
}

void OwnershipShapeChangesAndFallbacks() {
    const auto floor = Shape::Box({4, .3f, 4}), box = Shape::Box({.5f, .5f, .5f});
    Episode episode;
    Check(floor, Pose({0, 0, 0}), box, Pose({0, .75f, 0}), episode, "old owner seed");
    episode.Reset(); // Owner removed/reassigned; root graph must perform this.
    Require(!episode.initialized && episode.separatingAxis < 0 && !episode.face.uncutFour,
            "explicit owner/generation reset removes retained features");
    const auto before = persistent_contact_geometry::GetStats();
    Check(box, Pose({0, 0, 0}), floor, Pose({0, -.75f, 0}), episode,
          "replacement owner and reversed directed pair");
    Require(persistent_contact_geometry::GetStats().signatureResets > before.signatureResets,
            "replacement begins a fresh shape signature");
    Check(box, Pose({0, 0, 0}), Shape::Box({.7f, .4f, .3f}), Pose({.9f, 0, 0}), episode,
          "runtime dimension replacement");
    Check(box, Pose({0, 0, 0}), Shape::Sphere(.5f), Pose({.9f, 0, 0}), episode,
          "sphere uses original analytic dispatcher");
    Require(!episode.initialized, "unsupported acceleration type does not retain box episode");
    Check(box, Pose({0, 0, 0}), box, Pose({1.01f, 0, 0}), episode,
          "positive-margin query remains native", ShadowOptions(), .02f);
    Options disabled;
    disabled.separatingWitness = disabled.uncutFaceReplay = false;
    Check(box, Pose({0, 0, 0}), box, Pose({.9f, 0, 0}), episode,
          "both features disabled is native control", disabled);
}

void OracleSensitivity() {
    const auto a = Shape::Box({4, .3f, 4}), b = Shape::Box({.5f, .5f, .5f});
    const auto native = PrimitiveContacts(a, Pose({0, 0, 0}), b, Pose({0, .75f, 0}));
    Require(native.count > 0, "oracle corruption control has real contact data");
    if (!native.count)
        return;
    auto corrupted = native;
    corrupted.points[0].signedSeparation = std::nextafter(
        corrupted.points[0].signedSeparation, std::numeric_limits<double>::infinity());
    Require(!SameManifold(native, corrupted), "oracle detects a one-ULP separation corruption");
    corrupted = native;
    corrupted.points[0].localWitnessA.x = std::nextafter(
        corrupted.points[0].localWitnessA.x, std::numeric_limits<double>::infinity());
    Require(!SameManifold(native, corrupted), "oracle detects a one-ULP witness corruption");
    corrupted = native;
    corrupted.uncertain = !corrupted.uncertain;
    Require(!SameManifold(native, corrupted), "oracle detects uncertainty policy corruption");
    Require(!SameManifold(native, ContactManifold{}), "oracle detects stale separated answer");
}
} // namespace

int main() {
    persistent_contact_geometry::ResetStats();
    OracleSensitivity();
    FaceContinuationAndExits();
    SeparationContactAndDegeneracy();
    ExactBoundaryAndNonunitOrientation();
    DeterministicMovingPairsAndCrossAxis();
    OwnershipShapeChangesAndFallbacks();
    const auto stats = persistent_contact_geometry::GetStats();
    Require(stats.shadowChecks > 0 && stats.certifiedSeparated > 0 && stats.faceReplay > 0,
            "both shortcut mechanisms produced independently checked proposals");
    Require(stats.shadowMismatches == 0, "no proposed geometry disagrees with complete native output");
    std::cout << "checks=" << checks << " failures=" << failures << " queries=" << observedQueries
              << " shadow_checks=" << stats.shadowChecks
              << " shadow_mismatches=" << stats.shadowMismatches
              << " hints=" << stats.hintAttempts << " certified=" << stats.certifiedSeparated
              << " face_replay=" << stats.faceReplay << " native_sat=" << stats.nativeSatLoops
              << " feature_changes=" << stats.faceChanged << " face_exits=" << stats.faceOutside
              << " gap_exits=" << stats.faceGapRejected << '\n';
    return failures ? 1 : 0;
}
