#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "core/ITree.h"
#include "layout/Layout.h"

struct TreeDescriptor {
    // Идентификатор дерева в протоколе: avl, rb, treap, splay, btree.
    std::string id;
    LayoutKind layout;
};

// Все деревья протокола в фиксированном порядке, включая ещё не подключённые.
const std::vector<TreeDescriptor>& allTrees();

const TreeDescriptor* findTree(const std::string& id);

// nullptr — реализация дерева ещё не подключена к сборке.
std::unique_ptr<ITree> createTree(const std::string& id, std::uint64_t seed);
