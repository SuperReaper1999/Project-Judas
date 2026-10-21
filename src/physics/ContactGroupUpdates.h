#pragma once
#include <algorithm>
#include <cstddef>
#include <map>
#include <utility>
#include <vector>

namespace JudasPhysicsDetail {
// Step-local updates to a sorted row store. Ordering defines a primitive group;
// generation-sensitive policy remains with the owner. No solver sees staged
// rows. Untouched groups are not scanned or copied for each physical event.
template <class Row> class ContactGroupUpdates {
    using Key = decltype(std::declval<Row>().key);
    struct Patch {
        std::size_t first, last;
        std::vector<Row> rows;
    };
    std::vector<Row> & original;
    std::map<Key, Patch> patches;
    auto Span(const Key & key) const {
        const auto first = std::lower_bound(original.cbegin(), original.cend(), key,
            [](const Row & row, const Key & value) { return row.key < value; });
        const auto last = std::upper_bound(first, original.cend(), key,
            [](const Key & value, const Row & row) { return value < row.key; });
        return std::make_pair(first, last);
    }
public:
    std::size_t copiedRows = 0, publishedRows = 0;
    explicit ContactGroupUpdates(std::vector<Row> & rows) : original(rows) {}
    ContactGroupUpdates(const ContactGroupUpdates &) = delete;
    ContactGroupUpdates & operator=(const ContactGroupUpdates &) = delete;
    ~ContactGroupUpdates() {
        Publish();
    }
    bool Empty() const { return patches.empty(); }
    auto Read(const Key & key) const {
        const auto found = patches.find(key);
        return found == patches.end() ? Span(key)
            : std::make_pair(found->second.rows.cbegin(), found->second.rows.cend());
    }
    std::vector<Row> & Edit(const Key & key) {
        const auto found = patches.find(key);
        if (found != patches.end())
            return found->second.rows;
        const auto range = Span(key);
        Patch patch{std::size_t(range.first - original.cbegin()),
                    std::size_t(range.second - original.cbegin()),
                    std::vector<Row>(range.first, range.second)};
        copiedRows += patch.rows.size();
        return patches.emplace(key, std::move(patch)).first->second.rows;
    }
    void Publish() {
        if (patches.empty())
            return;
        std::size_t count = original.size();
        for (const auto & item : patches)
            count = count - (item.second.last - item.second.first) + item.second.rows.size();
        std::vector<Row> merged;
        merged.reserve(count);
        std::size_t first = 0;
        for (const auto & item : patches) {
            const auto & patch = item.second;
            merged.insert(merged.end(), original.cbegin() + first, original.cbegin() + patch.first);
            merged.insert(merged.end(), patch.rows.cbegin(), patch.rows.cend());
            first = patch.last;
        }
        merged.insert(merged.end(), original.cbegin() + first, original.cend());
        publishedRows += merged.size();
        original.swap(merged);
        patches.clear();
    }
};
}
