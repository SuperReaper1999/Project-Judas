#include "physics/ImpactSupportSets.h"

#include <iostream>
#include <random>
#include <stdexcept>
#include <string>

using JudasPhysicsDetail::ImpactSupportKey;
using JudasPhysicsDetail::SetImpactSupport;

namespace {
using Keys = std::vector<ImpactSupportKey>;

void OriginalUpdate(Keys &support, Keys &spinning, const ImpactSupportKey &key, bool spin) {
    auto &into = spin ? spinning : support;
    auto &from = spin ? support : spinning;
    from.erase(std::remove(from.begin(), from.end(), key), from.end());
    into.insert(std::upper_bound(into.begin(), into.end(), key), key);
    into.erase(std::unique(into.begin(), into.end()), into.end());
}

ImpactSupportKey Key(unsigned value) {
    return {value / 4, value / 4 + 1, value % 2, (value / 2) % 2};
}
} // namespace

int main(int argc, char **argv) {
    const bool omitErase = argc > 1 && std::string(argv[1]) == "--omit-erase";
    std::mt19937 random(17791);
    unsigned checks = 0, failures = 0;
    for (int trial = 0; trial < 12000; ++trial) {
        Keys support, spinning, events;
        std::vector<bool> classes;
        const unsigned population = random() % 120;
        for (unsigned i = 0; i < population; ++i) {
            if (random() % 3 == 0)
                support.push_back(Key(i));
            if (random() % 3 == 0)
                spinning.push_back(Key(i));
        }
        std::sort(support.begin(), support.end());
        std::sort(spinning.begin(), spinning.end());
        for (unsigned count = random() % 180; count; --count) {
            events.push_back(Key(random() % 150));
            classes.push_back(random() % 2);
        }
        // Includes repeated keys that change class, keys initially present in
        // both lists, empty lists and a predicate throwing at any prefix.
        const int stop = trial % 2 || events.empty() ? -1 : int(random() % events.size());
        auto expectedSupport = support, expectedSpinning = spinning;
        bool oldError = false, newError = false;
        try {
            for (std::size_t i = 0; i < events.size(); ++i) {
                if (int(i) == stop)
                    throw std::runtime_error("classification failure");
                OriginalUpdate(expectedSupport, expectedSpinning, events[i], classes[i]);
            }
        } catch (const std::runtime_error &) {
            oldError = true;
        }
        try {
            for (std::size_t i = 0; i < events.size(); ++i) {
                if (int(i) == stop)
                    throw std::runtime_error("classification failure");
                if (omitErase) {
                    auto &into = classes[i] ? spinning : support;
                    into.insert(std::upper_bound(into.begin(), into.end(), events[i]), events[i]);
                    into.erase(std::unique(into.begin(), into.end()), into.end());
                } else {
                    SetImpactSupport(support, spinning, events[i], classes[i]);
                }
            }
        } catch (const std::runtime_error &) {
            newError = true;
        }
        for (bool ok : {oldError == newError, support == expectedSupport,
                        spinning == expectedSpinning, std::is_sorted(support.begin(), support.end()),
                        std::is_sorted(spinning.begin(), spinning.end())}) {
            ++checks;
            failures += !ok;
        }
    }
    std::cout << checks << " checks, " << failures << " failures\n";
    return failures ? 1 : 0;
}
