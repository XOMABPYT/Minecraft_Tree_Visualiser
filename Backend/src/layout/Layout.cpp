#include "layout/Layout.h"

#include <algorithm>
#include <cstddef>

namespace {

class LayoutBuilder {
public:
    LayoutBuilder(const TreeSnapshot& snapshot, LayoutKind kind)
        : snapshot_(snapshot), kind_(kind), visited_(snapshot.nodes.size(), false) {}

    TreeLayout build() {
        if (isValidChild(snapshot_.root)) {
            if (kind_ == LayoutKind::Binary) {
                placeBinary(snapshot_.root, 0);
            } else {
                placeMultiway(snapshot_.root, 0);
            }
        }
        return std::move(layout_);
    }

private:
    // Индексы вне диапазона и повторные посещения игнорируются, чтобы
    // некорректный снимок не приводил к зацикливанию.
    bool isValidChild(int index) const {
        return index >= 0 && static_cast<std::size_t>(index) < snapshot_.nodes.size() &&
               !visited_[static_cast<std::size_t>(index)];
    }

    std::size_t enter(int index, int depth) {
        visited_[static_cast<std::size_t>(index)] = true;
        layout_.nodes.push_back({index, 0, depth});
        return layout_.nodes.size() - 1;
    }

    void placeBinary(int index, int depth) {
        const std::size_t placed = enter(index, depth);
        const auto& children = snapshot_.nodes[static_cast<std::size_t>(index)].children;
        for (std::size_t slot = 0; slot < children.size(); ++slot) {
            // Колонка узла — между левым поддеревом и остальными.
            if (slot == 1) {
                layout_.nodes[placed].x = nextColumn_++;
            }
            const int child = children[slot];
            if (!isValidChild(child)) {
                continue;
            }
            layout_.edges.push_back({index, child, static_cast<int>(slot)});
            placeBinary(child, depth + 1);
        }
        if (children.size() < 2) {
            layout_.nodes[placed].x = nextColumn_++;
        }
    }

    struct Span {
        int x;
        int width;
    };

    Span placeMultiway(int index, int depth) {
        const std::size_t placed = enter(index, depth);
        const auto& node = snapshot_.nodes[static_cast<std::size_t>(index)];
        const int width = std::max<int>(1, static_cast<int>(node.keys.size()));

        bool hasChildren = false;
        int left = 0;
        int right = 0;
        for (std::size_t slot = 0; slot < node.children.size(); ++slot) {
            const int child = node.children[slot];
            if (!isValidChild(child)) {
                continue;
            }
            layout_.edges.push_back({index, child, static_cast<int>(slot)});
            const Span span = placeMultiway(child, depth + 1);
            if (!hasChildren) {
                left = span.x;
                hasChildren = true;
            }
            right = span.x + span.width - 1;
        }

        int x;
        if (hasChildren) {
            x = left + (right - left + 1 - width) / 2;
        } else {
            x = nextColumn_;
        }
        nextColumn_ = std::max(nextColumn_, x + width + kMultiwayLeafGap);
        layout_.nodes[placed].x = x;
        return {x, width};
    }

    const TreeSnapshot& snapshot_;
    LayoutKind kind_;
    std::vector<bool> visited_;
    TreeLayout layout_;
    int nextColumn_ = 0;
};

}  // namespace

TreeLayout computeLayout(const TreeSnapshot& snapshot, LayoutKind kind) {
    return LayoutBuilder(snapshot, kind).build();
}
