#pragma once

// Private observer, never a cache. Every wrapped query still executes natively.
// One physics owner thread, one observed world, Reset() before each Step().
#include "Contacts.h"
#include "RigidBody.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <ostream>
#include <string>
#include <unordered_map>

namespace ccd_audit {

enum class Origin : unsigned { Unknown, InitialSchedule, Reschedule, Domain, Support, Other };
inline thread_local Origin currentOrigin = Origin::Unknown;

class OriginScope {
  public:
    explicit OriginScope(Origin origin) : previous(currentOrigin) { currentOrigin = origin; }
    ~OriginScope() { currentOrigin = previous; }
    OriginScope(const OriginScope &) = delete;
    OriginScope &operator=(const OriginScope &) = delete;

  private:
    Origin previous;
};

enum class Kind : unsigned { Manifold, Touches, PenetrationExceeds };
constexpr unsigned kKindCount = 3, kOriginCount = 6;
constexpr std::size_t kHardKeyLimit = 2000000;
inline const char *KindName(Kind kind) {
    return std::array<const char *, 3>{"manifold", "touches", "penetration"}[unsigned(kind)];
}
inline const char *OriginName(unsigned origin) {
    return std::array<const char *, 6>{"unknown", "initial", "reschedule", "domain", "support",
                                       "other"}[origin];
}

inline std::uint32_t Bits(float value) {
    std::uint32_t result;
    std::memcpy(&result, &value, sizeof(result));
    return result;
}
inline std::uint64_t Bits(double value) {
    std::uint64_t result;
    std::memcpy(&result, &value, sizeof(result));
    return result;
}
inline std::uint64_t Now() {
    return std::uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(
                             std::chrono::steady_clock::now().time_since_epoch())
                             .count());
}

template <std::size_t N> struct Key {
    std::array<std::uint32_t, N> words{};
    bool operator==(const Key &other) const { return words == other.words; }
};
template <std::size_t N> struct KeyHash {
    std::size_t operator()(const Key<N> &key) const {
        std::uint64_t hash = 1469598103934665603ull;
        for (auto word : key.words) {
            hash ^= word;
            hash *= 1099511628211ull;
        }
        return std::size_t(hash);
    }
};

// Canonical identity, but reversed call order is retained as a separate bit:
// clipping order and normal orientation are part of exact directed output.
using PairKey = Key<7>;
using GeometryKey = Key<25>;
inline bool Reversed(unsigned a, unsigned b, int pa, int pb) {
    return a > b || (a == b && pa > pb);
}
inline PairKey MakePair(unsigned a, unsigned b, unsigned ga, unsigned gb, int pa, int pb) {
    const bool reversed = Reversed(a, b, pa, pb);
    if (reversed) {
        std::swap(a, b);
        std::swap(ga, gb);
        std::swap(pa, pb);
    }
    return {{{a, b, ga, gb, std::uint32_t(pa), std::uint32_t(pb), std::uint32_t(reversed)}}};
}
inline void PutPose(std::uint32_t *out, const RigidBody &pose) {
    const std::array<float, 7> values{pose.position.x, pose.position.y, pose.position.z,
                                     pose.orientation.w, pose.orientation.x,
                                     pose.orientation.y, pose.orientation.z};
    for (unsigned i = 0; i < values.size(); ++i)
        out[i] = Bits(values[i]);
}
inline GeometryKey MakeGeometry(Kind kind, unsigned a, unsigned b, unsigned ga, unsigned gb,
                                int pa, int pb, const RigidBody &aa, const RigidBody &bb,
                                float margin, double target) {
    GeometryKey key;
    const auto pair = MakePair(a, b, ga, gb, pa, pb);
    std::copy(pair.words.begin(), pair.words.end(), key.words.begin());
    const bool reversed = Reversed(a, b, pa, pb);
    PutPose(key.words.data() + 7, reversed ? bb : aa);
    PutPose(key.words.data() + 14, reversed ? aa : bb);
    key.words[21] = Bits(margin);
    const auto threshold = Bits(target);
    key.words[22] = std::uint32_t(threshold);
    key.words[23] = std::uint32_t(threshold >> 32);
    key.words[24] = unsigned(kind);
    return key;
}

// Two stable hashes, explicitly adding physical and presentation fields. No
// native struct bytes/padding, no unused manifold slots, no large map payload.
struct Digest {
    std::uint64_t first = 1469598103934665603ull, second = 1099511628211ull;
    void Add(std::uint64_t value) {
        first = (first ^ value) * 1099511628211ull;
        second ^= value + 0x9e3779b97f4a7c15ull + (second << 6) + (second >> 2);
    }
    void Add(float value) { Add(std::uint64_t(Bits(value))); }
    void Add(double value) { Add(Bits(value)); }
    void AddVector(glm::vec3 value) { Add(value.x); Add(value.y); Add(value.z); }
    void AddVector(glm::dvec3 value) { Add(value.x); Add(value.y); Add(value.z); }
    bool operator==(const Digest &other) const {
        return first == other.first && second == other.second;
    }
};
inline Digest ResultDigest(const ContactManifold &manifold) {
    Digest hash;
    hash.Add(std::uint64_t(manifold.count));
    hash.Add(std::uint64_t(manifold.uncertain));
    for (int i = 0; i < manifold.count; ++i) {
        const auto &point = manifold.points[i];
        hash.Add(std::uint64_t(point.hit));
        hash.AddVector(point.point);
        hash.AddVector(point.normal);
        hash.Add(point.penetration);
        hash.Add(point.signedSeparation);
        hash.AddVector(point.preciseNormal);
        hash.AddVector(point.localAnchorA);
        hash.AddVector(point.localAnchorB);
        hash.AddVector(point.localWitnessA);
        hash.AddVector(point.localWitnessB);
        hash.Add(std::uint64_t(point.hasLocalAnchors));
        hash.Add(std::uint64_t(point.anchorAInWorldFrame));
        hash.Add(std::uint64_t(point.anchorBInWorldFrame));
        hash.Add(std::uint64_t(point.separationState));
    }
    return hash;
}

struct Counters {
    std::uint64_t calls = 0, unique = 0, repeat = 0, sameAnchor = 0, crossAnchor = 0;
    std::uint64_t trueResults = 0, falseResults = 0, repeatTrue = 0, repeatFalse = 0;
    std::uint64_t points = 0, repeatPoints = 0, uncertain = 0, mismatch = 0, untracked = 0, aborted = 0;
    std::uint64_t zeroMargin = 0, positiveMargin = 0, repeatZeroMargin = 0, repeatPositiveMargin = 0;
    std::uint64_t nativeNs = 0, repeatedNativeNs = 0, newNativeNs = 0;
};
struct QueryEntry {
    Digest result;
    std::uint64_t firstAnchor = 0;
};
struct RequestSnapshot {
    GeometryKey geometry;
    std::array<std::uint32_t, 12> velocity{};
    std::array<std::uint32_t, 40> motion{};
    std::uint64_t start = 0, finish = 0, target = 0, anchor = 0, result = 0;
};
struct RequestCounters {
    std::uint64_t calls = 0, captured = 0, earlyBounds = 0, uncaptured = 0, aborted = 0;
    std::uint64_t hit = 0, noImpact = 0, previousPair = 0, sameInput = 0;
    std::uint64_t sameInputCrossAnchor = 0, sameInputMismatch = 0;
    std::uint64_t sameGeometry = 0, changedVelocitySameGeometry = 0;
    std::uint64_t changedIntervalSameGeometry = 0, changedMotionAnchorSameGeometry = 0;
    std::uint64_t untracked = 0, nativeInclusiveNs = 0;
};
struct TruthCounters {
    std::uint64_t hit = 0, miss = 0, hitTrue = 0, hitFalse = 0, missTrue = 0, missFalse = 0;
};
struct State {
    bool enabled = false, timing = false, configured = false;
    std::size_t keyLimit = 262144;
    std::array<Counters, kKindCount> queries;
    std::array<std::array<Counters, kKindCount>, kOriginCount> origins;
    RequestCounters requests;
    std::array<RequestCounters, kOriginCount> originRequests;
    TruthCounters truth;
    std::array<TruthCounters, kOriginCount> originTruth;
    std::unordered_map<GeometryKey, QueryEntry, KeyHash<25>> geometry;
    std::unordered_map<PairKey, RequestSnapshot, KeyHash<7>> lastRequest;
    std::uint64_t overflow = 0, observerNs = 0, clockCalls = 0, clockPairFloorNs = 0;
};
inline State state;

inline bool Enabled() { return state.enabled; }
inline void SetEnabled(bool enabled) {
    if (enabled && !state.configured) {
        state.configured = true;
        if (const auto *setting = std::getenv("JUDAS_CCD_AUDIT_TIMING"))
            state.timing = std::strtoull(setting, nullptr, 10) != 0;
        if (const auto *setting = std::getenv("JUDAS_CCD_AUDIT_MAX_KEYS"))
            state.keyLimit = std::clamp<std::size_t>(std::strtoull(setting, nullptr, 10), 1,
                                                    kHardKeyLimit);
        if (state.timing) {
            state.clockPairFloorNs = std::numeric_limits<std::uint64_t>::max();
            for (unsigned i = 0; i < 32; ++i) {
                const auto first = Now(), second = Now();
                state.clockPairFloorNs = std::min(state.clockPairFloorNs, second - first);
            }
        }
    }
    state.enabled = enabled;
}
inline void Reset() {
    state.queries = {};
    state.origins = {};
    state.requests = {};
    state.originRequests = {};
    state.truth = {};
    state.originTruth = {};
    state.geometry.clear();
    state.lastRequest.clear();
    state.overflow = state.observerNs = state.clockCalls = 0;
}
inline std::uint64_t Clock() {
    if (!state.timing)
        return 0;
    ++state.clockCalls;
    return Now();
}
inline void ChargeObserver(std::uint64_t begin) {
    if (state.timing)
        state.observerNs += Clock() - begin;
}
inline bool HasCapacity() {
    return state.geometry.size() + state.lastRequest.size() < state.keyLimit;
}

class QueryScope {
  public:
    QueryScope(Kind kind, unsigned a, unsigned b, unsigned ga, unsigned gb, int pa, int pb,
               const RigidBody &aa, const RigidBody &bb, double anchor, float margin = 0,
               double target = 0)
        : active(Enabled()), kind(kind), origin(currentOrigin), anchor(Bits(anchor)), margin(margin) {
        if (!active)
            return;
        const auto begin = Clock();
        key = MakeGeometry(kind, a, b, ga, gb, pa, pb, aa, bb, margin, target);
        ChargeObserver(begin);
        observerBefore = state.observerNs;
        nativeBegin = Clock();
    }
    ~QueryScope() {
        if (active && !finished) {
            ++state.queries[unsigned(kind)].aborted;
            ++state.origins[unsigned(origin)][unsigned(kind)].aborted;
        }
    }
    QueryScope(const QueryScope &) = delete;
    QueryScope &operator=(const QueryScope &) = delete;

    void Finish(bool value) {
        if (!active)
            return;
        const auto end = Clock();
        Digest result;
        result.Add(std::uint64_t(value));
        Record(result, value, 0, false, end);
    }
    void Finish(const ContactManifold &value) {
        if (!active)
            return;
        const auto end = Clock();
        Record(ResultDigest(value), value.count > 0, std::uint64_t(value.count), value.uncertain,
               end);
    }

  private:
    void Record(Digest result, bool value, std::uint64_t points, bool uncertain,
                std::uint64_t nativeEnd) {
        const auto nestedObserver = state.observerNs - observerBefore;
        const auto elapsed = nativeEnd - nativeBegin;
        const auto nativeNs = elapsed > nestedObserver ? elapsed - nestedObserver : 0;
        const auto bookkeepingBegin = nativeEnd;
        auto old = state.geometry.find(key);
        const bool repeated = old != state.geometry.end();
        const bool crossAnchor = repeated && old->second.firstAnchor != anchor;
        const bool mismatch = repeated && !(old->second.result == result);
        bool tracked = repeated;
        if (!repeated && HasCapacity()) {
            state.geometry.emplace(key, QueryEntry{result, anchor});
            tracked = true;
        }
        if (!tracked)
            ++state.overflow;
        const auto update = [&](Counters &counter) {
            ++counter.calls;
            counter.unique += !repeated && tracked;
            counter.repeat += repeated;
            counter.sameAnchor += repeated && !crossAnchor;
            counter.crossAnchor += crossAnchor;
            counter.trueResults += value;
            counter.falseResults += !value;
            counter.repeatTrue += repeated && value;
            counter.repeatFalse += repeated && !value;
            counter.points += points;
            counter.repeatPoints += repeated ? points : 0;
            counter.uncertain += uncertain;
            counter.mismatch += mismatch;
            counter.untracked += !tracked;
            counter.zeroMargin += margin == 0;
            counter.positiveMargin += margin > 0;
            counter.repeatZeroMargin += repeated && margin == 0;
            counter.repeatPositiveMargin += repeated && margin > 0;
            counter.nativeNs += nativeNs;
            counter.repeatedNativeNs += repeated ? nativeNs : 0;
            counter.newNativeNs += repeated ? 0 : nativeNs;
        };
        update(state.queries[unsigned(kind)]);
        update(state.origins[unsigned(origin)][unsigned(kind)]);
        finished = true;
        ChargeObserver(bookkeepingBegin);
    }
    bool active = false, finished = false;
    Kind kind;
    Origin origin;
    GeometryKey key;
    std::uint64_t anchor = 0, nativeBegin = 0, observerBefore = 0;
    float margin = 0;
};

inline void LocalTruth(bool hit, bool value) {
    if (!Enabled())
        return;
    const auto begin = Clock();
    const auto update = [&](TruthCounters &counter) {
        counter.hit += hit;
        counter.miss += !hit;
        counter.hitTrue += hit && value;
        counter.hitFalse += hit && !value;
        counter.missTrue += !hit && value;
        counter.missFalse += !hit && !value;
    };
    update(state.truth);
    update(state.originTruth[unsigned(currentOrigin)]);
    ChargeObserver(begin);
}

// Preserve the exact future geometric segment anchor. Same current pose/rate
// alone does NOT imply the same future binary32 path after a ledger restart.
template <class Body> inline void PutMotion(std::uint32_t *out, const Body &body, double time) {
    std::fill(out, out + 20, 0);
    out[0] = body.HasMotion();
    if (!body.HasMotion())
        return;
    const auto &segments = body.motion.Segments();
    auto segment = std::upper_bound(segments.begin(), segments.end(), time,
                                    [](double at, const auto &value) { return at < value.begin; });
    if (segment == segments.begin()) {
        out[0] |= 2;
        return;
    }
    --segment;
    const auto begin = Bits(segment->begin), finish = Bits(segment->end);
    out[1] = std::uint32_t(begin);
    out[2] = std::uint32_t(begin >> 32);
    out[3] = std::uint32_t(finish);
    out[4] = std::uint32_t(finish >> 32);
    const std::array<float, 13> floats{
        segment->position.x, segment->position.y, segment->position.z,
        segment->orientation.w, segment->orientation.x, segment->orientation.y,
        segment->orientation.z, segment->linearVelocity.x, segment->linearVelocity.y,
        segment->linearVelocity.z, segment->driftAngularVelocity.x,
        segment->driftAngularVelocity.y, segment->driftAngularVelocity.z};
    for (unsigned i = 0; i < floats.size(); ++i)
        out[5 + i] = Bits(floats[i]);
    out[18] = segment->prescribed;
    out[19] = Bits(segment->inverseMass);
}

class ImpactScope {
  public:
    ImpactScope(double start, double finish, double target, double anchor)
        : active(Enabled()), origin(currentOrigin), finishTime(finish) {
        if (!active)
            return;
        snapshot.start = Bits(start);
        snapshot.finish = Bits(finish);
        snapshot.target = Bits(target);
        snapshot.anchor = Bits(anchor);
        observerBefore = state.observerNs;
        nativeBegin = Clock();
    }
    ~ImpactScope() {
        if (active && !finished) {
            ++state.requests.aborted;
            ++state.originRequests[unsigned(origin)].aborted;
        }
    }
    ImpactScope(const ImpactScope &) = delete;
    ImpactScope &operator=(const ImpactScope &) = delete;
    void EarlyBoundReject() { if (active) earlyBounds = true; }

    template <class Body>
    void Capture(unsigned a, unsigned b, int pa, int pb, const Body &first, const Body &second,
                 const RigidBody &aa, const RigidBody &bb, double start) {
        if (!active)
            return;
        const auto begin = Clock();
        captured = true;
        pair = MakePair(a, b, first.generation, second.generation, pa, pb);
        snapshot.geometry = MakeGeometry(Kind::Manifold, a, b, first.generation, second.generation,
                                         pa, pb, aa, bb, 0, 0);
        const bool reversed = Reversed(a, b, pa, pb);
        const auto &one = reversed ? bb : aa;
        const auto &two = reversed ? aa : bb;
        const std::array<float, 12> velocities{
            one.linearVelocity.x, one.linearVelocity.y, one.linearVelocity.z,
            one.angularVelocity.x, one.angularVelocity.y, one.angularVelocity.z,
            two.linearVelocity.x, two.linearVelocity.y, two.linearVelocity.z,
            two.angularVelocity.x, two.angularVelocity.y, two.angularVelocity.z};
        for (unsigned i = 0; i < velocities.size(); ++i)
            snapshot.velocity[i] = Bits(velocities[i]);
        PutMotion(snapshot.motion.data(), reversed ? second : first, start);
        PutMotion(snapshot.motion.data() + 20, reversed ? first : second, start);
        ChargeObserver(begin);
    }
    void Finish(double result) {
        if (!active)
            return;
        const auto end = Clock();
        const auto gross = end - nativeBegin, nestedObserver = state.observerNs - observerBefore;
        const auto nativeNs = gross > nestedObserver ? gross - nestedObserver : 0;
        snapshot.result = Bits(result);
        const auto old = captured ? state.lastRequest.find(pair) : state.lastRequest.end();
        const bool previous = old != state.lastRequest.end();
        const bool geometry = previous && old->second.geometry == snapshot.geometry;
        const bool velocity = previous && old->second.velocity == snapshot.velocity;
        const bool motion = previous && old->second.motion == snapshot.motion;
        const bool interval = previous && old->second.start == snapshot.start &&
                              old->second.finish == snapshot.finish;
        const bool same = geometry && velocity && motion && interval &&
                          old->second.target == snapshot.target;
        const bool mismatch = same && old->second.result != snapshot.result;
        const bool crossAnchor = same && old->second.anchor != snapshot.anchor;
        bool tracked = previous;
        if (captured && (previous || HasCapacity())) {
            state.lastRequest[pair] = snapshot;
            tracked = true;
        }
        if (captured && !tracked)
            ++state.overflow;
        const auto update = [&](RequestCounters &counter) {
            ++counter.calls;
            counter.captured += captured;
            counter.earlyBounds += earlyBounds;
            counter.uncaptured += !captured;
            counter.hit += result <= finishTime;
            counter.noImpact += !(result <= finishTime);
            counter.previousPair += previous;
            counter.sameInput += same;
            counter.sameInputCrossAnchor += crossAnchor;
            counter.sameInputMismatch += mismatch;
            counter.sameGeometry += geometry;
            counter.changedVelocitySameGeometry += geometry && !velocity;
            counter.changedIntervalSameGeometry += geometry && !interval;
            counter.changedMotionAnchorSameGeometry += geometry && !motion;
            counter.untracked += captured && !tracked;
            counter.nativeInclusiveNs += nativeNs;
        };
        update(state.requests);
        update(state.originRequests[unsigned(origin)]);
        finished = true;
        ChargeObserver(end);
    }

  private:
    bool active = false, captured = false, earlyBounds = false, finished = false;
    Origin origin;
    double finishTime;
    PairKey pair;
    RequestSnapshot snapshot;
    std::uint64_t nativeBegin = 0, observerBefore = 0;
};

template <class Emit> inline void Fields(Emit emit) {
    const auto query = [&](const char *prefix, const Counters &q) {
        emit(prefix, "calls", q.calls); emit(prefix, "unique", q.unique);
        emit(prefix, "repeat", q.repeat); emit(prefix, "same_anchor", q.sameAnchor);
        emit(prefix, "cross_anchor", q.crossAnchor); emit(prefix, "true", q.trueResults);
        emit(prefix, "false", q.falseResults); emit(prefix, "repeat_true", q.repeatTrue);
        emit(prefix, "repeat_false", q.repeatFalse); emit(prefix, "points", q.points);
        emit(prefix, "repeat_points", q.repeatPoints);
        emit(prefix, "uncertain", q.uncertain); emit(prefix, "mismatch", q.mismatch);
        emit(prefix, "untracked", q.untracked); emit(prefix, "aborted", q.aborted);
        emit(prefix, "zero_margin", q.zeroMargin); emit(prefix, "positive_margin", q.positiveMargin);
        emit(prefix, "repeat_zero_margin", q.repeatZeroMargin);
        emit(prefix, "repeat_positive_margin", q.repeatPositiveMargin);
        emit(prefix, "native_ns", q.nativeNs); emit(prefix, "repeat_native_ns", q.repeatedNativeNs);
        emit(prefix, "new_native_ns", q.newNativeNs);
    };
    const auto request = [&](const char *prefix, const RequestCounters &r) {
        emit(prefix, "calls", r.calls); emit(prefix, "captured", r.captured);
        emit(prefix, "early_bounds", r.earlyBounds); emit(prefix, "uncaptured", r.uncaptured);
        emit(prefix, "aborted", r.aborted); emit(prefix, "hit", r.hit);
        emit(prefix, "no_impact", r.noImpact); emit(prefix, "previous_pair", r.previousPair);
        emit(prefix, "same_input", r.sameInput);
        emit(prefix, "same_input_cross_anchor", r.sameInputCrossAnchor);
        emit(prefix, "same_input_mismatch", r.sameInputMismatch);
        emit(prefix, "same_geometry", r.sameGeometry);
        emit(prefix, "changed_velocity_same_geometry", r.changedVelocitySameGeometry);
        emit(prefix, "changed_interval_same_geometry", r.changedIntervalSameGeometry);
        emit(prefix, "changed_motion_anchor_same_geometry", r.changedMotionAnchorSameGeometry);
        emit(prefix, "untracked", r.untracked); emit(prefix, "native_inclusive_ns", r.nativeInclusiveNs);
    };
    const auto truth = [&](const char *prefix, const TruthCounters &t) {
        emit(prefix, "hit", t.hit); emit(prefix, "miss", t.miss);
        emit(prefix, "hit_true", t.hitTrue); emit(prefix, "hit_false", t.hitFalse);
        emit(prefix, "miss_true", t.missTrue); emit(prefix, "miss_false", t.missFalse);
    };
    for (unsigned kind = 0; kind < kKindCount; ++kind)
        query(KindName(Kind(kind)), state.queries[kind]);
    request("impact", state.requests);
    truth("local_truth", state.truth);
    for (unsigned origin = 0; origin < kOriginCount; ++origin) {
        for (unsigned kind = 0; kind < kKindCount; ++kind) {
            const std::string prefix = std::string(OriginName(origin)) + "_" + KindName(Kind(kind));
            query(prefix.c_str(), state.origins[origin][kind]);
        }
        const std::string requestPrefix = std::string(OriginName(origin)) + "_impact";
        request(requestPrefix.c_str(), state.originRequests[origin]);
        const std::string truthPrefix = std::string(OriginName(origin)) + "_local_truth";
        truth(truthPrefix.c_str(), state.originTruth[origin]);
    }
}
inline void Header(std::ostream &out) {
    out << "step,enabled,timing,key_limit,geometry_keys,request_pairs,overflow,observer_ns,clock_calls,clock_pair_floor_ns";
    Fields([&](const char *prefix, const char *field, std::uint64_t) {
        out << ',' << prefix << '_' << field;
    });
    out << '\n';
}
inline void WriteStep(std::ostream &out, std::uint64_t step) {
    out << step << ',' << state.enabled << ',' << state.timing << ',' << state.keyLimit << ','
        << state.geometry.size() << ',' << state.lastRequest.size() << ',' << state.overflow << ','
        << state.observerNs << ',' << state.clockCalls << ',' << state.clockPairFloorNs;
    Fields([&](const char *, const char *, std::uint64_t value) { out << ',' << value; });
    out << '\n';
}

} // namespace ccd_audit
