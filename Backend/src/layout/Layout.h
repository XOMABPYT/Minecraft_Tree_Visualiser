#pragma once

#include <vector>

#include "core/TreeSnapshot.h"

enum class LayoutKind {
    // Один ключ в узле, x — номер узла в in-order обходе.
    Binary,
    // Узел занимает keys.size() соседних клеток; листья идут слева направо
    // с промежутком, родитель центрируется над детьми.
    Multiway,
};

struct PlacedNode {
    // Индекс узла в TreeSnapshot::nodes.
    int node;
    // Колонка первой клетки узла в условной сетке.
    int x;
    // Уровень, корень = 0.
    int depth;
};

struct LayoutEdge {
    int from;
    int to;
    // Номер ребёнка в SnapshotNode::children.
    int slot;
};

struct TreeLayout {
    // Только узлы, достижимые из корня, в порядке обхода в глубину.
    std::vector<PlacedNode> nodes;
    std::vector<LayoutEdge> edges;
};

// Промежуток в клетках между соседними листьями в Multiway-раскладке.
constexpr int kMultiwayLeafGap = 1;

TreeLayout computeLayout(const TreeSnapshot& snapshot, LayoutKind kind);
