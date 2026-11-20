#pragma once

#include <string>
#include <utility>
#include <vector>

#include "Key.h"

// Описание формы дерева, не зависящее от его реализации.
// Раскладка и протокол работают только с ним.
struct SnapshotNode {
    std::vector<Key> keys;
    // Индексы детей в TreeSnapshot::nodes, -1 — ребёнка нет.
    // У бинарных деревьев всегда два элемента: левый и правый.
    // У B-дерева children[i] — поддерево между keys[i - 1] и keys[i].
    std::vector<int> children;
    // Одно из: white, red, black, gray, blue, green, yellow.
    std::string color = "white";
    std::vector<std::string> label;
    std::vector<std::pair<std::string, std::string>> info;
};

struct TreeSnapshot {
    std::vector<SnapshotNode> nodes;
    // -1 — дерево пустое.
    int root = -1;
};
