#include "world/TreeRegistry.h"

#include "Treap.h"

const std::vector<TreeDescriptor>& allTrees() {
    static const std::vector<TreeDescriptor> trees = {
        {"avl", LayoutKind::Binary},
        {"rb", LayoutKind::Binary},
        {"treap", LayoutKind::Binary},
        {"splay", LayoutKind::Binary},
        {"btree", LayoutKind::Multiway},
    };
    return trees;
}

const TreeDescriptor* findTree(const std::string& id) {
    for (const auto& tree : allTrees()) {
        if (tree.id == id) {
            return &tree;
        }
    }
    return nullptr;
}

std::unique_ptr<ITree> createTree(const std::string& id, std::uint64_t seed) {
    if (id == "treap") {
        return std::make_unique<Treap>(seed);
    }
    // avl, rb, splay и btree подключаются здесь, когда их реализации готовы.
    return nullptr;
}
