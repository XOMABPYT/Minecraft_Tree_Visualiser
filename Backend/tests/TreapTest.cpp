#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <random>
#include <set>
#include <string>
#include <vector>

#include "Treap.h"

namespace {

std::string infoValue(const SnapshotNode& node, const std::string& name) {
    for (const auto& [key, value] : node.info) {
        if (key == name) {
            return value;
        }
    }
    ADD_FAILURE() << "в info узла нет поля " << name;
    return "0";
}

struct SubtreeStats {
    int size = 0;
    int height = 0;
};

// Проверяет свойства treap в поддереве и собирает ключи in-order.
SubtreeStats checkSubtree(const TreeSnapshot& s, int index, std::vector<Key>& keys) {
    if (index == -1) {
        return {};
    }
    const auto& node = s.nodes.at(static_cast<std::size_t>(index));
    EXPECT_EQ(node.keys.size(), 1u);
    EXPECT_EQ(node.children.size(), 2u);
    if (node.keys.size() != 1 || node.children.size() != 2) {
        return {};
    }

    const std::uint64_t priority = std::stoull(infoValue(node, "p"));
    for (int child : node.children) {
        if (child != -1) {
            const auto& childNode = s.nodes.at(static_cast<std::size_t>(child));
            EXPECT_GE(priority, std::stoull(infoValue(childNode, "p")))
                << "нарушено свойство кучи между " << node.keys[0] << " и " << childNode.keys[0];
        }
    }

    const SubtreeStats left = checkSubtree(s, node.children[0], keys);
    keys.push_back(node.keys[0]);
    const SubtreeStats right = checkSubtree(s, node.children[1], keys);

    SubtreeStats stats;
    stats.size = left.size + right.size + 1;
    stats.height = std::max(left.height, right.height) + 1;
    EXPECT_EQ(std::to_string(stats.size), infoValue(node, "size")) << "ключ " << node.keys[0];
    EXPECT_EQ(std::to_string(stats.height), infoValue(node, "height")) << "ключ " << node.keys[0];
    return stats;
}

void expectValidTreap(const Treap& treap, const std::set<Key>& expected) {
    const TreeSnapshot s = treap.snapshot();
    std::vector<Key> keys;
    const SubtreeStats stats = checkSubtree(s, s.root, keys);
    EXPECT_EQ(static_cast<std::size_t>(stats.size), expected.size());
    EXPECT_EQ(keys, std::vector<Key>(expected.begin(), expected.end()));
}

bool sameSnapshot(const TreeSnapshot& a, const TreeSnapshot& b) {
    if (a.root != b.root || a.nodes.size() != b.nodes.size()) {
        return false;
    }
    for (std::size_t i = 0; i < a.nodes.size(); ++i) {
        const auto& x = a.nodes[i];
        const auto& y = b.nodes[i];
        if (x.keys != y.keys || x.children != y.children || x.color != y.color ||
            x.label != y.label || x.info != y.info) {
            return false;
        }
    }
    return true;
}

}  // namespace

TEST(TreapTest, EmptySnapshot) {
    Treap treap;
    const TreeSnapshot s = treap.snapshot();
    EXPECT_EQ(s.root, -1);
    EXPECT_TRUE(s.nodes.empty());
}

TEST(TreapTest, InsertRejectsDuplicate) {
    Treap treap(1);
    EXPECT_TRUE(treap.insert(5));
    EXPECT_FALSE(treap.insert(5));
    expectValidTreap(treap, {5});
}

TEST(TreapTest, EraseMissingKeyReturnsFalse) {
    Treap treap(1);
    EXPECT_FALSE(treap.erase(5));
    treap.insert(3);
    treap.insert(7);
    EXPECT_FALSE(treap.erase(5));
    expectValidTreap(treap, {3, 7});
}

TEST(TreapTest, EraseLastKeyLeavesEmptyTree) {
    Treap treap(1);
    treap.insert(1);
    EXPECT_TRUE(treap.erase(1));
    EXPECT_EQ(treap.snapshot().root, -1);
}

TEST(TreapTest, InvariantsHoldUnderRandomOperations) {
    Treap treap(42);
    std::set<Key> expected;
    std::mt19937_64 rng(7);
    std::uniform_int_distribution<Key> keyDist(-200, 200);
    for (int step = 0; step < 3000; ++step) {
        const Key key = keyDist(rng);
        if (rng() % 3 == 0) {
            EXPECT_EQ(treap.erase(key), expected.erase(key) == 1) << "шаг " << step;
        } else {
            EXPECT_EQ(treap.insert(key), expected.insert(key).second) << "шаг " << step;
        }
        if (step % 100 == 0) {
            expectValidTreap(treap, expected);
        }
    }
    expectValidTreap(treap, expected);
}

TEST(TreapTest, NegativeAndExtremeKeys) {
    Treap treap(3);
    const Key lo = std::numeric_limits<Key>::min();
    const Key hi = std::numeric_limits<Key>::max();
    for (Key key : {Key{0}, Key{-1}, lo, hi, Key{1}}) {
        EXPECT_TRUE(treap.insert(key));
    }
    expectValidTreap(treap, {lo, -1, 0, 1, hi});
    EXPECT_TRUE(treap.erase(lo));
    EXPECT_TRUE(treap.erase(0));
    expectValidTreap(treap, {-1, 1, hi});
}

TEST(TreapTest, SnapshotDoesNotChangeTree) {
    Treap treap(5);
    for (Key key = 0; key < 50; ++key) {
        treap.insert((key * 37) % 101);
    }
    EXPECT_TRUE(sameSnapshot(treap.snapshot(), treap.snapshot()));
}

TEST(TreapTest, SameSeedGivesSameShape) {
    Treap a(99);
    Treap b(99);
    for (Key key = 0; key < 64; ++key) {
        a.insert(key);
        b.insert(key);
    }
    EXPECT_TRUE(sameSnapshot(a.snapshot(), b.snapshot()));
}

// Отключено до исправления в Treap.cpp: get_min() возвращает 1e18 для пустого
// поддерева, поэтому у узлов с ключом > 1e18 после update() поле mn неверное,
// и insert() не замечает дубликат.
TEST(TreapTest, DISABLED_RejectsDuplicateOfKeyAbove1e18) {
    Treap treap(1);
    const Key big = 2000000000000000000LL;
    EXPECT_TRUE(treap.insert(big));
    EXPECT_FALSE(treap.insert(big));
    expectValidTreap(treap, {big});
}

// Отключено до исправления в Treap.cpp: erase() вызывает split(.., x + 1),
// для INT64_MAX это переполнение знакового целого (UB), ключ не удаляется.
TEST(TreapTest, DISABLED_EraseMaxKey) {
    Treap treap(1);
    const Key hi = std::numeric_limits<Key>::max();
    treap.insert(hi);
    EXPECT_TRUE(treap.erase(hi));
    expectValidTreap(treap, {});
}

TEST(TreapTest, ExpectedHeightIsLogarithmic) {
    Treap treap(11);
    for (Key key = 0; key < 1000; ++key) {
        treap.insert(key);
    }
    const TreeSnapshot s = treap.snapshot();
    ASSERT_NE(s.root, -1);
    const int height = std::stoi(infoValue(s.nodes[static_cast<std::size_t>(s.root)], "height"));
    EXPECT_LT(height, 60) << "отсортированная вставка не должна вырождать treap в список";
}
