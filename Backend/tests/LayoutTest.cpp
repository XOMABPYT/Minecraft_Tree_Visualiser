#include <gtest/gtest.h>

#include <algorithm>
#include <map>
#include <random>
#include <set>
#include <utility>
#include <vector>

#include "Treap.h"
#include "layout/Layout.h"

namespace {

int addNode(TreeSnapshot& s, std::vector<Key> keys, std::vector<int> children = {}) {
    SnapshotNode node;
    node.keys = std::move(keys);
    node.children = std::move(children);
    s.nodes.push_back(node);
    return static_cast<int>(s.nodes.size()) - 1;
}

std::map<int, PlacedNode> byNode(const TreeLayout& layout) {
    std::map<int, PlacedNode> result;
    for (const auto& placed : layout.nodes) {
        EXPECT_TRUE(result.emplace(placed.node, placed).second) << "узел размещён дважды";
    }
    return result;
}

// Каждая клетка (x, depth) занята не более чем одним ключом.
void expectNoOverlap(const TreeSnapshot& s, const TreeLayout& layout) {
    std::set<std::pair<int, int>> cells;
    for (const auto& placed : layout.nodes) {
        const int width = std::max<int>(
            1, static_cast<int>(s.nodes[static_cast<std::size_t>(placed.node)].keys.size()));
        for (int i = 0; i < width; ++i) {
            EXPECT_TRUE(cells.emplace(placed.x + i, placed.depth).second)
                << "перекрытие в клетке x=" << placed.x + i << " depth=" << placed.depth;
        }
    }
}

// Случайное дерево формы B-дерева с минимальной степенью t.
int buildBTreeShape(TreeSnapshot& s, std::mt19937& rng, int levels, bool isRoot, int t) {
    std::uniform_int_distribution<int> keyCount(isRoot ? 1 : t - 1, 2 * t - 1);
    const int count = keyCount(rng);
    std::vector<Key> keys(static_cast<std::size_t>(count), 0);
    std::vector<int> children;
    if (levels > 1) {
        for (int i = 0; i <= count; ++i) {
            children.push_back(buildBTreeShape(s, rng, levels - 1, false, t));
        }
    }
    return addNode(s, keys, children);
}

}  // namespace

TEST(LayoutTest, EmptySnapshot) {
    TreeSnapshot s;
    EXPECT_TRUE(computeLayout(s, LayoutKind::Binary).nodes.empty());
    EXPECT_TRUE(computeLayout(s, LayoutKind::Multiway).nodes.empty());
}

TEST(LayoutTest, BinaryUsesInOrderIndex) {
    //       20
    //      /  \
    //    10    30
    //      \
    //      15
    TreeSnapshot s;
    const int n15 = addNode(s, {15}, {-1, -1});
    const int n10 = addNode(s, {10}, {-1, n15});
    const int n30 = addNode(s, {30}, {-1, -1});
    const int n20 = addNode(s, {20}, {n10, n30});
    s.root = n20;

    const TreeLayout layout = computeLayout(s, LayoutKind::Binary);
    const auto placed = byNode(layout);
    ASSERT_EQ(placed.size(), 4u);
    EXPECT_EQ(placed.at(n10).x, 0);
    EXPECT_EQ(placed.at(n15).x, 1);
    EXPECT_EQ(placed.at(n20).x, 2);
    EXPECT_EQ(placed.at(n30).x, 3);
    EXPECT_EQ(placed.at(n20).depth, 0);
    EXPECT_EQ(placed.at(n10).depth, 1);
    EXPECT_EQ(placed.at(n30).depth, 1);
    EXPECT_EQ(placed.at(n15).depth, 2);

    ASSERT_EQ(layout.edges.size(), 3u);
    std::set<std::tuple<int, int, int>> edges;
    for (const auto& e : layout.edges) {
        edges.emplace(e.from, e.to, e.slot);
    }
    EXPECT_EQ(edges.count({n20, n10, 0}), 1u);
    EXPECT_EQ(edges.count({n20, n30, 1}), 1u);
    EXPECT_EQ(edges.count({n10, n15, 1}), 1u);
    expectNoOverlap(s, layout);
}

TEST(LayoutTest, BinaryIgnoresInvalidAndRepeatedChildren) {
    TreeSnapshot s;
    const int leaf = addNode(s, {1}, {-1, 42});
    const int root = addNode(s, {2}, {leaf, leaf});
    s.nodes[static_cast<std::size_t>(leaf)].children[0] = root;
    s.root = root;

    const TreeLayout layout = computeLayout(s, LayoutKind::Binary);
    EXPECT_EQ(layout.nodes.size(), 2u);
    EXPECT_EQ(layout.edges.size(), 1u);
}

TEST(LayoutTest, BinaryLayoutOfTreapMatchesSortedOrder) {
    Treap treap(17);
    std::mt19937 rng(3);
    for (int i = 0; i < 200; ++i) {
        treap.insert(static_cast<Key>(rng() % 1000));
    }
    const TreeSnapshot s = treap.snapshot();
    const TreeLayout layout = computeLayout(s, LayoutKind::Binary);
    ASSERT_EQ(layout.nodes.size(), s.nodes.size());

    std::vector<std::pair<int, Key>> columns;
    for (const auto& placed : layout.nodes) {
        columns.emplace_back(placed.x, s.nodes[static_cast<std::size_t>(placed.node)].keys[0]);
    }
    std::sort(columns.begin(), columns.end());
    for (std::size_t i = 0; i < columns.size(); ++i) {
        EXPECT_EQ(columns[i].first, static_cast<int>(i));
        if (i > 0) {
            EXPECT_LT(columns[i - 1].second, columns[i].second);
        }
    }

    const auto placed = byNode(layout);
    for (const auto& e : layout.edges) {
        EXPECT_EQ(placed.at(e.to).depth, placed.at(e.from).depth + 1);
    }
    expectNoOverlap(s, layout);
}

TEST(LayoutTest, MultiwayPlacesLeavesWithGapAndCentersParent) {
    //          [10 20]
    //   [1 2]  [12 15 17]  [25]
    TreeSnapshot s;
    const int a = addNode(s, {1, 2});
    const int b = addNode(s, {12, 15, 17});
    const int c = addNode(s, {25});
    const int root = addNode(s, {10, 20}, {a, b, c});
    s.root = root;

    const TreeLayout layout = computeLayout(s, LayoutKind::Multiway);
    const auto placed = byNode(layout);
    EXPECT_EQ(placed.at(a).x, 0);
    EXPECT_EQ(placed.at(b).x, 2 + kMultiwayLeafGap);
    EXPECT_EQ(placed.at(c).x, 5 + 2 * kMultiwayLeafGap);
    // Дети занимают клетки [0, 7], родитель шириной 2 — по центру.
    EXPECT_EQ(placed.at(root).x, 3);
    EXPECT_EQ(placed.at(root).depth, 0);
    EXPECT_EQ(placed.at(c).depth, 1);

    ASSERT_EQ(layout.edges.size(), 3u);
    for (std::size_t i = 0; i < layout.edges.size(); ++i) {
        EXPECT_EQ(layout.edges[i].from, root);
        EXPECT_EQ(layout.edges[i].slot, static_cast<int>(i));
    }
    expectNoOverlap(s, layout);
}

TEST(LayoutTest, MultiwayRandomBTreesDoNotOverlap) {
    std::mt19937 rng(5);
    for (int round = 0; round < 50; ++round) {
        TreeSnapshot s;
        s.root = buildBTreeShape(s, rng, 1 + round % 4, true, 3);
        const TreeLayout layout = computeLayout(s, LayoutKind::Multiway);
        ASSERT_EQ(layout.nodes.size(), s.nodes.size());
        expectNoOverlap(s, layout);

        const auto placed = byNode(layout);
        for (std::size_t i = 0; i < s.nodes.size(); ++i) {
            const auto& node = s.nodes[i];
            if (node.children.empty()) {
                continue;
            }
            const PlacedNode& first = placed.at(node.children.front());
            const PlacedNode& last = placed.at(node.children.back());
            const int lastWidth =
                static_cast<int>(s.nodes[static_cast<std::size_t>(node.children.back())].keys.size());
            const PlacedNode& self = placed.at(static_cast<int>(i));
            EXPECT_GE(self.x, first.x);
            EXPECT_LE(self.x + static_cast<int>(node.keys.size()), last.x + lastWidth);
        }
    }
}
