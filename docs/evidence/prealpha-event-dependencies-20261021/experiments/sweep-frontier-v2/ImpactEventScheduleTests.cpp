#include "physics/ImpactEventSchedule.h"
#include "physics/ImpactSweepIndex.h"
#include <cstdio>
#include <random>
#include <map>
#include <string>

using namespace JudasPhysicsDetail;
namespace {
unsigned checks = 0, failures = 0;
void Check(bool value, const char* label) {
    ++checks;
    if (!value) { ++failures; std::printf("FAIL %s\n", label); }
}
struct Event { std::array<unsigned, 4> key; double time; };
}
int main(int argc, char** argv) {
    const bool negative = argc == 2 && std::string(argv[1]) == "--omit-update";
    std::vector<Event> events;
    ImpactEventSchedule<Event> schedule(events, 100);
    std::map<std::array<unsigned, 4>, double> oracle;
    std::mt19937 rng(9471);
    // Arbitrary updates, ties, compound-child keys and retired events. The
    // oracle deliberately uses a full independent ordered scan, not a heap.
    for (unsigned i = 0; i < 20000; ++i) {
        const std::array<unsigned, 4> key{unsigned(rng()%30), unsigned(30+rng()%30), unsigned(rng()%4), unsigned(rng()%3)};
        const double time = (rng()%100)/100.;
        schedule.Set(key, time); oracle[key] = time;
        auto first = oracle.begin();
        for (auto it = oracle.begin(); it != oracle.end(); ++it)
            if (it->second < first->second) first = it;
        Check(schedule.First()->key == first->first && schedule.First()->time == first->second,
              "indexed frontier exactly matches independent chronological/key oracle");
    }
    std::vector<std::array<unsigned, 4>> expected, actual;
    for (const auto& entry : oracle)
        if ((entry.first[0] == 12 || entry.first[1] == 12) && entry.second <= .5)
            expected.push_back(entry.first);
    schedule.ForIncident(12, .5, [&](const auto& key) { actual.push_back(key); });
    std::sort(actual.begin(), actual.end());
    Check(actual == expected, "only incident pending events are invalidated");
    Check(!schedule.Contains({99,99,99,99}), "absent event is not fabricated");

    ImpactSweepIndex sweep(3);
    const Aabb left{{-2,-1,-1},{-1,1,1}}, right{{1,-1,-1},{2,1,1}};
    sweep.Update(0,left); sweep.Update(1,right); sweep.Update(2,{{4,4,4},{5,5,5}});
    Check(sweep.Separated(0,1), "disjoint complete enclosures certify no impact");
    sweep.Update(0,{{-1.9f,-.9f,-.9f},{-1.1f,.9f,.9f}});
    Check(sweep.Separated(0,1), "changed path contained in certificate safely retains separation");
    // Both paths escape their former certificates into a new shared region.
    // Publishing only one endpoint is intentionally a detectable broken control.
    sweep.Update(0,{{8,-1,-1},{10,1,1}});
    if (!negative) sweep.Update(1,{{9,-1,-1},{11,1,1}});
    Check(!sweep.Separated(0,1), "escaping paths invalidate no-impact certificate before discovery");
    const auto candidates = sweep.Neighbours(0);
    Check(std::find(candidates.begin(),candidates.end(),1) != candidates.end(),
          "new collision outside both original bounds is discovered");
    if (negative) {
        std::printf("negative control: %u expected failures\n", failures);
        return failures == 2 ? 0 : 1;
    }
    sweep.Update(1,{{10,-1,-1},{12,1,1}});
    Check(!sweep.Separated(0,1), "inclusive tangency is never rejected");
    // Numerous arbitrary orientations are represented by their independently
    // supplied AABBs. Check that all actual overlaps remain candidates while
    // shrinking paths may conservatively retain extra candidates.
    ImpactSweepIndex many(64);
    std::vector<Aabb> bounds(64);
    for (unsigned round=0; round<20; ++round) {
        for (unsigned i=0; i<64; ++i) {
            glm::vec3 centre(float(int(rng()%100)-50)*.01f,
                             float(int(rng()%100)-50)*.01f,
                             float(int(rng()%100)-50)*.01f);
            bounds[i] = {centre-glm::vec3(.08f),centre+glm::vec3(.08f)};
            many.Update(i,bounds[i]);
        }
        for (unsigned i=0;i<64;++i) {
            const auto ids=many.Neighbours(i);
            for (unsigned j=0;j<64;++j)
                if (bounds[i].Overlaps(bounds[j]))
                    Check(std::binary_search(ids.begin(),ids.end(),j),
                          "brute-force overlap never lost through incremental publication");
        }
    }
    std::printf("%u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
