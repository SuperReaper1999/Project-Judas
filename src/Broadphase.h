#pragma once

#include <vector>

#include <glm/glm.hpp>

// Milestone 32: Judas's rigid-body broadphase.
//
// An axis-aligned bounding box in simulation space. World axes are used here
// only as a bounding-volume coordinate system — an AABB of a rotated body is
// still the exact bound of that body, and rotating the whole world changes
// which pairs are *candidates* (bounds are conservative) but never which
// pairs the narrowphase reports as touching. No axis is "up".
struct Aabb {
    glm::vec3 min{0.0f};
    glm::vec3 max{0.0f};

    bool Overlaps(const Aabb& other) const {
        return min.x <= other.max.x && max.x >= other.min.x &&
               min.y <= other.max.y && max.y >= other.min.y &&
               min.z <= other.max.z && max.z >= other.min.z;
    }
    bool Contains(const Aabb& inner) const {
        return min.x <= inner.min.x && min.y <= inner.min.y && min.z <= inner.min.z &&
               max.x >= inner.max.x && max.y >= inner.max.y && max.z >= inner.max.z;
    }
    Aabb Union(const Aabb& other) const {
        return Aabb{glm::min(min, other.min), glm::max(max, other.max)};
    }
    Aabb Expanded(float margin) const {
        return Aabb{min - glm::vec3(margin), max + glm::vec3(margin)};
    }
    // Half the surface area: the insertion cost heuristic only compares
    // areas, so the constant factor is irrelevant.
    float Perimeter() const {
        const glm::vec3 d = max - min;
        return d.x * d.y + d.y * d.z + d.z * d.x;
    }
};

// A dynamic AABB tree (the bounding-volume hierarchy of Box2D/Bullet's
// "dbvt" family): every proxy is a leaf holding a *fat* AABB — the body's
// tight bound grown by a margin — so a body that moves a little inside its
// fat box costs nothing, and one that escapes it is removed and reinserted.
// Internal nodes bound their two children; insertion picks the sibling that
// minimises added surface area and AVL-style rotations keep the height
// logarithmic.
//
// Why this structure and not a grid or sweep-and-prune: Judas's bodies span
// four orders of magnitude in size (an 80 m terrain planet, 20 m spheres, a
// 40 m plank, 0.25 m crates) in one scene, which defeats a uniform grid's
// single cell size; sweep-and-prune sorts along world axes and degrades when
// many bodies share a coordinate (a 1,500-crate floor all at one height).
// The tree is indifferent to both, answers arbitrary box queries (reused by
// the player's capsule sweep and PhysicsWorld::QueryBodiesInAabb), and handles
// static and dynamic proxies in one structure.
//
// Candidate generation only: a reported overlap means "the narrowphase must
// look"; it never means "these bodies touch".
class DynamicAabbTree {
public:
    static constexpr int kNull = -1;

    int CreateProxy(const Aabb& fatAabb, unsigned int userData);
    void DestroyProxy(int proxy);
    // Replaces a proxy's fat AABB (remove + reinsert). The caller decides
    // when that is necessary (see PhysicsWorld: only when the tight bound
    // has escaped the current fat one).
    void MoveProxy(int proxy, const Aabb& fatAabb);

    const Aabb& FatAabb(int proxy) const { return m_nodes[static_cast<std::size_t>(proxy)].aabb; }
    unsigned int UserData(int proxy) const { return m_nodes[static_cast<std::size_t>(proxy)].userData; }

    // Calls `callback(proxy)` for every leaf whose fat AABB overlaps `aabb`.
    // Visit order depends on tree shape; callers needing determinism sort.
    template <typename Callback>
    void Query(const Aabb& aabb, Callback&& callback) const {
        if (m_root == kNull) return;
        std::vector<int>& stack = m_queryStack;
        stack.clear();
        stack.push_back(m_root);
        while (!stack.empty()) {
            const int nodeId = stack.back();
            stack.pop_back();
            const Node& node = m_nodes[static_cast<std::size_t>(nodeId)];
            if (!node.aabb.Overlaps(aabb)) continue;
            if (node.IsLeaf()) {
                callback(nodeId);
            } else {
                stack.push_back(node.child1);
                stack.push_back(node.child2);
            }
        }
    }

    int Height() const;
    int ProxyCount() const { return m_proxyCount; }
    // Structural self-check for tests: parent links, bounds containment,
    // heights and leaf count. Returns false on the first inconsistency.
    bool Validate() const;

private:
    struct Node {
        Aabb aabb;
        unsigned int userData = 0;
        int parent = kNull;  // doubles as the free-list link
        int child1 = kNull;
        int child2 = kNull;
        int height = -1;     // leaf = 0, free = -1
        bool IsLeaf() const { return child1 == kNull; }
    };

    int AllocateNode();
    void FreeNode(int node);
    void InsertLeaf(int leaf);
    void RemoveLeaf(int leaf);
    int Balance(int node);
    bool ValidateNode(int node, int& leafCount) const;

    std::vector<Node> m_nodes;
    int m_root = kNull;
    int m_freeList = kNull;
    int m_proxyCount = 0;
    mutable std::vector<int> m_queryStack;
};
