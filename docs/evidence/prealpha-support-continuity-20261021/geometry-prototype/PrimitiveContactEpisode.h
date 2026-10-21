#pragma once

// PRIVATE prototype. Pair identity/lifecycle is owned by the caller. The state
// records geometric feature hints, never an old Boolean or old manifold answer.
#include "Contacts.h"

#include <array>
#include <cstdint>

namespace persistent_contact_geometry {

struct FaceFeature {
    int axis = -1, incident = -1;
    bool referencePositive = false, incidentPositive = false, uncutFour = false;
};

struct Episode {
    // Caller must generation-validate endpoints and primitive identity, and keep
    // one directed order. Dimension changes are also detected by this signature.
    std::array<std::uint32_t, 10> signature{};
    bool initialized = false;
    int separatingAxis = -1;
    FaceFeature face;
    void Reset() { *this = {}; }
};

struct Options {
    bool separatingWitness = true;
    bool uncutFaceReplay = true;
    // Re-evaluate proposals using the complete native path; return native output.
    // This is diagnostic validation, not a performance measurement mode.
    bool shadowNative = false;
};

struct Stats {
    std::uint64_t queries = 0, unsupported = 0, signatureResets = 0;
    std::uint64_t boxSatEntries = 0, nativeSatLoops = 0, hintAttempts = 0;
    std::uint64_t certifiedSeparated = 0, uncertifiedHints = 0;
    std::uint64_t nativeSeparatedLearned = 0;
    std::uint64_t faceCandidates = 0, freshUncutFeatures = 0, faceReplay = 0;
    std::uint64_t faceChanged = 0, faceOutside = 0, faceGapRejected = 0;
    std::uint64_t uncertain = 0, shadowChecks = 0, shadowMismatches = 0;
};

Stats GetStats();
void ResetStats();

// Only zero-margin native Box/Box is accelerated. Other margins/types run the
// unchanged PrimitiveContacts path. No change in physical acceptance tolerance.
ContactManifold Query(const Shape &a, const ContactPose &pa, const Shape &b,
                      const ContactPose &pb, Episode &episode, Options options = {},
                      float margin = 0,
                      const ContactPreparedOrientation *preparedA = nullptr,
                      const ContactPreparedOrientation *preparedB = nullptr);

namespace detail {
struct Request {
    Episode &episode;
    Options options;
    bool proposed = false;
};
inline thread_local Request *current = nullptr;
inline thread_local Stats stats;

class Scope {
  public:
    explicit Scope(Request *request) : previous(current) { current = request; }
    ~Scope() { current = previous; }
    Scope(const Scope &) = delete;
    Scope &operator=(const Scope &) = delete;

  private:
    Request *previous;
};
} // namespace detail
} // namespace persistent_contact_geometry
