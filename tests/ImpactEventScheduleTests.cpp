#include "physics/ImpactEventSchedule.h"
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
    ImpactEventSchedule<Event> schedule(events, 100, .5);
    std::map<std::array<unsigned, 4>, double> oracle;
    std::mt19937 rng(9471);
    // Arbitrary updates, ties, compound-child keys and retired events. The
    // oracle deliberately uses a full independent ordered scan, not a heap.
    for (unsigned i = 0; i < 20000; ++i) {
        const std::array<unsigned, 4> key{unsigned(rng()%30), unsigned(30+rng()%30), unsigned(rng()%4), unsigned(rng()%3)};
        const double time = (rng()%100)/100.;
        schedule.Set(key, time); oracle[key] = time;
        auto first = oracle.end();
        for (auto it = oracle.begin(); it != oracle.end(); ++it)
            if (it->second <= .5 && (first == oracle.end() || it->second < first->second)) first = it;
        Check(first == oracle.end() ? schedule.First() == nullptr
                                   : schedule.First() && schedule.First()->key == first->first &&
                                         schedule.First()->time == first->second,
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

    std::vector<Event> isolated;
    ImpactEventSchedule<Event> pending(isolated, 10, 1.);
    pending.Set({0,1,0,0}, .4);
    pending.Set({2,3,0,0}, .6);
    pending.Set({4,5,0,0}, 2.);
    std::vector<std::array<unsigned,4>> incident;
    pending.ForIncident(4,1.,[&](const auto& key){incident.push_back(key);});
    Check(incident.empty(), "certified no-impact result has no pending-event dependency");
    if (!negative) pending.Set({0,1,0,0}, 2.);
    Check(pending.First()->key == std::array<unsigned,4>{2,3,0,0},
          "retired impact is removed without losing unrelated earlier event");
    incident.clear(); pending.ForIncident(0,1.,[&](const auto& key){incident.push_back(key);});
    Check(incident.empty(), "retirement removes both endpoint dependencies");
    if (negative) {
        std::printf("negative control: %u expected failures\n", failures);
        return failures == 2 ? 0 : 1;
    }
    pending.Set({4,5,0,0},.2);
    Check(pending.First()->key == std::array<unsigned,4>{4,5,0,0},
          "previously absent impact can reenter frontier after motion change");
    pending.Set({4,5,0,0}, 2.);
    pending.Set({2,3,0,0}, 2.);
    Check(pending.First() == nullptr, "frontier becomes empty when every impact is retired");
    pending.Set({7,7,1,2},.1); pending.Set({7,7,1,2},2.);
    incident.clear(); pending.ForIncident(7,1.,[&](const auto& key){incident.push_back(key);});
    Check(incident.empty() && !pending.First(), "self-endpoint bookkeeping does not duplicate incidence");
    std::printf("%u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
