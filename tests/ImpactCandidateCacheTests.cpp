#include "physics/ImpactCandidateCache.h"
#include <algorithm>
#include <cstdio>
#include <map>
#include <string>

using JudasPhysicsDetail::ImpactCandidateCache;

unsigned checks = 0, failures = 0;
void Check(bool value, const char *label) {
    ++checks;
    failures += !value;
    std::printf("%s %s\n", value ? "PASS" : "FAIL", label);
}
int main(int argc, char **argv) {
    const bool negative = argc == 2 && std::string(argv[1]) == "--omit-revision";
    DynamicAabbTree tree;
    std::map<unsigned, int> proxies;
    auto add = [&](unsigned slot, const Aabb &box) { proxies[slot] = tree.CreateProxy(box, slot); };
    add(11, {{-2, -2, -2}, {2, 2, 2}});
    add(3, {{1, -1, -1}, {3, 1, 1}});
    add(700001, {{7, -1, -1}, {8, 1, 1}});
    add(90, {{1.5f, -.2f, -.2f}, {1.8f, .2f, .2f}});
    std::size_t revision = 1, queries = 0;
    std::vector<unsigned> scratch;
    auto query = [&](const Aabb &box) -> const std::vector<unsigned> & {
        ++queries;
        scratch.clear();
        tree.Query(box, [&](int proxy) { scratch.push_back(tree.UserData(proxy)); });
        std::sort(scratch.begin(), scratch.end());
        return scratch;
    };
    auto bounds = [&](unsigned slot) -> const Aabb & { return tree.FatAabb(proxies.at(slot)); };
    ImpactCandidateCache cache;
    auto get = [&](unsigned slot, const Aabb &box) {
        return std::vector<unsigned>(cache.Get(slot, bounds(slot), box, revision, query, bounds));
    };
    auto oracle = [&](const Aabb &box) { return std::vector<unsigned>(query(box)); };
    const Aabb inside{{-1, -1, -1}, {1, 1, 1}};
    Check(get(11, inside) == oracle(inside),
          "contained query equals independently sorted tree output including tangency");
    const auto after = queries;
    get(11, inside);
    get(11, bounds(11));
    Check(queries == after, "unchanged global proxy revision reuses both tight and fat queries");
    Check(get(11, bounds(11)) == std::vector<unsigned>({3, 11, 90}),
          "exact ascending candidate identity order is preserved");
    const auto actualSubset = get(11, inside), fatSuperset = get(11, bounds(11));
    Check(std::find(actualSubset.begin(), actualSubset.end(), 90) == actualSubset.end() &&
              std::find(fatSuperset.begin(), fatSuperset.end(), 90) != fatSuperset.end(),
          "strict fat-only candidate is excluded by the actual subset query");
    ImpactCandidateCache tinySubset(1, 1);
    Check(std::vector<unsigned>(tinySubset.Get(11, bounds(11), inside, revision, query, bounds)) ==
              oracle(inside),
          "oversized fat superset falls back to the exact requested subset");
    const auto budgetQueries = queries;
    tinySubset.Get(11, bounds(11), inside, revision, query, bounds);
    Check(queries == budgetQueries + 1,
          "saturated budget avoids repeating the larger preliminary fat query");
    tree.MoveProxy(proxies.at(700001), {{0, -1, -1}, {.5f, 1, 1}});
    if (!negative)
        ++revision;
    Check(get(11, inside) == oracle(inside),
          "another proxy enters a cached query while querying body's own bound is unchanged");
    if (negative) {
        std::printf("SUMMARY %u checks %u failures (intentional missing-revision control)\n",
                    checks, failures);
        return failures ? 1 : 0;
    }
    tree.MoveProxy(proxies.at(3), {{20, -1, -1}, {21, 1, 1}});
    ++revision;
    Check(get(11, inside) == oracle(inside), "other-proxy departure invalidates cached candidates");
    tree.MoveProxy(proxies.at(11), {{6, -2, -2}, {10, 2, 2}});
    ++revision;
    Check(get(11, bounds(11)) == oracle(bounds(11)),
          "own proxy relocation reconstructs the complete candidate list");
    const Aabb outside{{19, -1, -1}, {22, 1, 1}};
    Check(get(11, outside) == oracle(outside),
          "query outside own enclosure falls back without omitting exterior bodies");
    tree.DestroyProxy(proxies.at(700001));
    proxies.erase(700001);
    add(700001, {{8, -1, -1}, {9, 1, 1}});
    ++revision;
    Check(get(11, bounds(11)) == oracle(bounds(11)),
          "destroy/create slot reuse with a new tree revision cannot retain retired membership");
    ImpactCandidateCache bounded(1, 1);
    const auto &fat = bounds(11);
    Check(std::vector<unsigned>(bounded.Get(11, fat, fat, revision, query, bounds)) == oracle(fat),
          "oversized candidate set retains uncached exact result");
    Check(bounded.CachedSlots() == 0 && bounded.RetainedIdCapacity() == 0,
          "retained ID budget does not allocate an oversized graph");
    ImpactCandidateCache noSlots(0, 16);
    Check(std::vector<unsigned>(noSlots.Get(11, fat, fat, revision, query, bounds)) == oracle(fat),
          "zero slot budget retains the original contained-query path");
    const auto result = get(11, fat);
    bool allow = true;
    auto filtered = [&]() {
        std::vector<unsigned> r;
        for (auto slot : get(11, fat))
            if (allow || slot != 700001)
                r.push_back(slot);
        return r;
    };
    Check(filtered() == result, "candidate caching does not prefilter current collision policy");
    allow = false;
    auto expected = result;
    expected.erase(std::remove(expected.begin(), expected.end(), 700001), expected.end());
    Check(filtered() == expected,
          "current response filtering remains changeable after a cache hit");
    Check(cache.CachedSlots() <= ImpactCandidateCache::MaximumSlots &&
              cache.RetainedIdCapacity() <= ImpactCandidateCache::MaximumRetainedIds,
          "sparse high identity does not allocate a body-square table or exceed cache bounds");
    std::printf("SUMMARY %u checks %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
