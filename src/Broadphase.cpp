#include "Broadphase.h"

#include <algorithm>
#include <cstdlib>

int DynamicAabbTree::AllocateNode() {
    if (m_freeList == kNull) {
        m_nodes.emplace_back();
        return static_cast<int>(m_nodes.size() - 1);
    }
    const int node = m_freeList;
    m_freeList = m_nodes[static_cast<std::size_t>(node)].parent;
    m_nodes[static_cast<std::size_t>(node)] = Node{};
    return node;
}

void DynamicAabbTree::FreeNode(int node) {
    Node& n = m_nodes[static_cast<std::size_t>(node)];
    n = Node{};
    n.parent = m_freeList;
    n.height = -1;
    m_freeList = node;
}

int DynamicAabbTree::CreateProxy(const Aabb& fatAabb, unsigned int userData) {
    const int leaf = AllocateNode();
    Node& n = m_nodes[static_cast<std::size_t>(leaf)];
    n.aabb = fatAabb;
    n.userData = userData;
    n.height = 0;
    InsertLeaf(leaf);
    ++m_proxyCount;
    return leaf;
}

void DynamicAabbTree::DestroyProxy(int proxy) {
    RemoveLeaf(proxy);
    FreeNode(proxy);
    --m_proxyCount;
}

void DynamicAabbTree::MoveProxy(int proxy, const Aabb& fatAabb) {
    RemoveLeaf(proxy);
    m_nodes[static_cast<std::size_t>(proxy)].aabb = fatAabb;
    InsertLeaf(proxy);
}

void DynamicAabbTree::InsertLeaf(int leaf) {
    if (m_root == kNull) {
        m_root = leaf;
        m_nodes[static_cast<std::size_t>(leaf)].parent = kNull;
        return;
    }

    // Descend choosing, at each internal node, whichever of "make a new
    // parent here" or "push into child 1/2" adds the least surface area
    // (the classic incremental surface-area heuristic).
    const Aabb leafAabb = m_nodes[static_cast<std::size_t>(leaf)].aabb;
    int index = m_root;
    while (!m_nodes[static_cast<std::size_t>(index)].IsLeaf()) {
        const Node& node = m_nodes[static_cast<std::size_t>(index)];
        const float area = node.aabb.Perimeter();
        const float combinedArea = node.aabb.Union(leafAabb).Perimeter();
        const float cost = 2.0f * combinedArea;
        const float inheritanceCost = 2.0f * (combinedArea - area);

        const auto childCost = [&](int childId) {
            const Node& child = m_nodes[static_cast<std::size_t>(childId)];
            const float unionArea = child.aabb.Union(leafAabb).Perimeter();
            return child.IsLeaf() ? unionArea + inheritanceCost
                                  : unionArea - child.aabb.Perimeter() + inheritanceCost;
        };
        const float cost1 = childCost(node.child1);
        const float cost2 = childCost(node.child2);
        if (cost < cost1 && cost < cost2) break;
        index = cost1 < cost2 ? node.child1 : node.child2;
    }

    const int sibling = index;
    const int oldParent = m_nodes[static_cast<std::size_t>(sibling)].parent;
    const int newParent = AllocateNode();
    {
        Node& p = m_nodes[static_cast<std::size_t>(newParent)];
        p.parent = oldParent;
        p.aabb = leafAabb.Union(m_nodes[static_cast<std::size_t>(sibling)].aabb);
        p.height = m_nodes[static_cast<std::size_t>(sibling)].height + 1;
        p.child1 = sibling;
        p.child2 = leaf;
    }
    if (oldParent != kNull) {
        Node& op = m_nodes[static_cast<std::size_t>(oldParent)];
        if (op.child1 == sibling) op.child1 = newParent;
        else op.child2 = newParent;
    } else {
        m_root = newParent;
    }
    m_nodes[static_cast<std::size_t>(sibling)].parent = newParent;
    m_nodes[static_cast<std::size_t>(leaf)].parent = newParent;

    // Walk back up refitting bounds and heights, rebalancing as we go.
    index = m_nodes[static_cast<std::size_t>(leaf)].parent;
    while (index != kNull) {
        index = Balance(index);
        Node& node = m_nodes[static_cast<std::size_t>(index)];
        const Node& c1 = m_nodes[static_cast<std::size_t>(node.child1)];
        const Node& c2 = m_nodes[static_cast<std::size_t>(node.child2)];
        node.height = 1 + std::max(c1.height, c2.height);
        node.aabb = c1.aabb.Union(c2.aabb);
        index = node.parent;
    }
}

void DynamicAabbTree::RemoveLeaf(int leaf) {
    if (leaf == m_root) {
        m_root = kNull;
        return;
    }
    const int parent = m_nodes[static_cast<std::size_t>(leaf)].parent;
    const int grandParent = m_nodes[static_cast<std::size_t>(parent)].parent;
    const int sibling = m_nodes[static_cast<std::size_t>(parent)].child1 == leaf
                            ? m_nodes[static_cast<std::size_t>(parent)].child2
                            : m_nodes[static_cast<std::size_t>(parent)].child1;
    if (grandParent != kNull) {
        Node& gp = m_nodes[static_cast<std::size_t>(grandParent)];
        if (gp.child1 == parent) gp.child1 = sibling;
        else gp.child2 = sibling;
        m_nodes[static_cast<std::size_t>(sibling)].parent = grandParent;
        FreeNode(parent);
        int index = grandParent;
        while (index != kNull) {
            index = Balance(index);
            Node& node = m_nodes[static_cast<std::size_t>(index)];
            const Node& c1 = m_nodes[static_cast<std::size_t>(node.child1)];
            const Node& c2 = m_nodes[static_cast<std::size_t>(node.child2)];
            node.aabb = c1.aabb.Union(c2.aabb);
            node.height = 1 + std::max(c1.height, c2.height);
            index = node.parent;
        }
    } else {
        m_root = sibling;
        m_nodes[static_cast<std::size_t>(sibling)].parent = kNull;
        FreeNode(parent);
    }
    m_nodes[static_cast<std::size_t>(leaf)].parent = kNull;
}

// One AVL rotation at node A if its subtrees differ in height by more than
// one; returns the index now occupying A's place.
int DynamicAabbTree::Balance(int iA) {
    Node* A = &m_nodes[static_cast<std::size_t>(iA)];
    if (A->IsLeaf() || A->height < 2) return iA;

    const int iB = A->child1;
    const int iC = A->child2;
    Node* B = &m_nodes[static_cast<std::size_t>(iB)];
    Node* C = &m_nodes[static_cast<std::size_t>(iC)];
    const int balance = C->height - B->height;

    const auto rotateUp = [&](int iX, Node* X, int iOther, bool xIsChild2) {
        // X (a child of A) is promoted into A's place.
        const int iF = X->child1;
        const int iG = X->child2;
        Node* F = &m_nodes[static_cast<std::size_t>(iF)];
        Node* G = &m_nodes[static_cast<std::size_t>(iG)];
        X->child1 = iA;
        X->parent = A->parent;
        A->parent = iX;
        if (X->parent != kNull) {
            Node& xp = m_nodes[static_cast<std::size_t>(X->parent)];
            if (xp.child1 == iA) xp.child1 = iX;
            else xp.child2 = iX;
        } else {
            m_root = iX;
        }
        Node* other = &m_nodes[static_cast<std::size_t>(iOther)];
        const auto attach = [&](int keep, Node* K, int give, Node* Gv) {
            X->child2 = keep;
            if (xIsChild2) A->child2 = give;
            else A->child1 = give;
            Gv->parent = iA;
            A->aabb = other->aabb.Union(Gv->aabb);
            X->aabb = A->aabb.Union(K->aabb);
            A->height = 1 + std::max(other->height, Gv->height);
            X->height = 1 + std::max(A->height, K->height);
        };
        if (F->height > G->height) attach(iF, F, iG, G);
        else attach(iG, G, iF, F);
        return iX;
    };

    if (balance > 1) return rotateUp(iC, C, iB, /*xIsChild2=*/true);
    if (balance < -1) return rotateUp(iB, B, iC, /*xIsChild2=*/false);
    return iA;
}

int DynamicAabbTree::Height() const {
    return m_root == kNull ? 0 : m_nodes[static_cast<std::size_t>(m_root)].height;
}

bool DynamicAabbTree::ValidateNode(int index, int& leafCount) const {
    const Node& node = m_nodes[static_cast<std::size_t>(index)];
    if (node.IsLeaf()) {
        ++leafCount;
        return node.height == 0 && node.child2 == kNull;
    }
    const Node& c1 = m_nodes[static_cast<std::size_t>(node.child1)];
    const Node& c2 = m_nodes[static_cast<std::size_t>(node.child2)];
    if (c1.parent != index || c2.parent != index) return false;
    if (node.height != 1 + std::max(c1.height, c2.height)) return false;
    if (std::abs(c1.height - c2.height) > 1) return false;
    if (!node.aabb.Contains(c1.aabb) || !node.aabb.Contains(c2.aabb)) return false;
    return ValidateNode(node.child1, leafCount) && ValidateNode(node.child2, leafCount);
}

bool DynamicAabbTree::Validate() const {
    if (m_root == kNull) return m_proxyCount == 0;
    if (m_nodes[static_cast<std::size_t>(m_root)].parent != kNull) return false;
    int leaves = 0;
    return ValidateNode(m_root, leaves) && leaves == m_proxyCount;
}
