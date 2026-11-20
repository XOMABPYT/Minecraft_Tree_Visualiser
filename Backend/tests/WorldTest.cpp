#include <gtest/gtest.h>

#include <algorithm>
#include <limits>
#include <set>
#include <vector>

#include "world/World.h"

namespace {

void collectInOrder(const TreeSnapshot& s, int index, std::vector<Key>& out) {
    if (index == -1) {
        return;
    }
    const auto& node = s.nodes.at(static_cast<std::size_t>(index));
    collectInOrder(s, node.children.at(0), out);
    out.push_back(node.keys.at(0));
    collectInOrder(s, node.children.at(1), out);
}

std::vector<Key> activeKeys(const World& world) {
    const TreeSnapshot s = world.activeSnapshot();
    std::vector<Key> keys;
    collectInOrder(s, s.root, keys);
    return keys;
}

std::vector<Key> setKeys(const World& world) {
    return {world.keys().begin(), world.keys().end()};
}

}  // namespace

TEST(WorldTest, TreapIsConnectedAndActiveByDefault) {
    World world;
    EXPECT_EQ(world.connectedTrees(), std::vector<std::string>{"treap"});
    EXPECT_EQ(world.activeTreeId(), "treap");
    EXPECT_EQ(world.activeLayoutKind(), LayoutKind::Binary);
    EXPECT_EQ(world.activeSnapshot().root, -1);
}

TEST(WorldTest, InsertAndErase) {
    World world;
    EXPECT_TRUE(world.insert(10).ok());
    EXPECT_TRUE(world.insert(-5).ok());
    EXPECT_TRUE(world.insert(7).ok());
    EXPECT_EQ(activeKeys(world), (std::vector<Key>{-5, 7, 10}));

    EXPECT_TRUE(world.erase(7).ok());
    EXPECT_EQ(activeKeys(world), (std::vector<Key>{-5, 10}));
    EXPECT_EQ(setKeys(world), (std::vector<Key>{-5, 10}));
}

TEST(WorldTest, DuplicateInsertFails) {
    World world;
    world.insert(1);
    const OpResult result = world.insert(1);
    EXPECT_EQ(result.error, ErrorCode::DuplicateKey);
    EXPECT_EQ(activeKeys(world), std::vector<Key>{1});
}

TEST(WorldTest, EraseMissingKeyFails) {
    World world;
    world.insert(1);
    EXPECT_EQ(world.erase(2).error, ErrorCode::KeyNotFound);
    EXPECT_EQ(activeKeys(world), std::vector<Key>{1});
}

TEST(WorldTest, ViewSwitchesOnlyToConnectedTrees) {
    World world;
    EXPECT_EQ(world.view("oak").error, ErrorCode::UnknownTree);
    EXPECT_EQ(world.view("avl").error, ErrorCode::UnknownTree);
    EXPECT_EQ(world.activeTreeId(), "treap");
    EXPECT_TRUE(world.view("treap").ok());
    EXPECT_EQ(world.activeTreeId(), "treap");
}

TEST(WorldTest, ClearRemovesAllKeys) {
    World world;
    world.insert(1);
    world.insert(2);
    world.clear();
    EXPECT_TRUE(world.keys().empty());
    EXPECT_EQ(world.activeSnapshot().root, -1);
    EXPECT_TRUE(world.insert(1).ok());
}

TEST(WorldTest, RandomInsertsDistinctNewKeysInRange) {
    World world;
    world.insert(5);
    const OpResult result = world.insertRandom(20, 0, 30);
    ASSERT_TRUE(result.ok()) << result.message;
    ASSERT_EQ(result.inserted.size(), 20u);
    std::set<Key> unique(result.inserted.begin(), result.inserted.end());
    EXPECT_EQ(unique.size(), 20u);
    EXPECT_EQ(unique.count(5), 0u);
    for (Key key : result.inserted) {
        EXPECT_GE(key, 0);
        EXPECT_LE(key, 30);
    }
    EXPECT_EQ(world.keys().size(), 21u);
    EXPECT_EQ(activeKeys(world), setKeys(world));
}

TEST(WorldTest, RandomCanFillWholeRange) {
    World world;
    world.insert(2);
    world.insert(4);
    const OpResult result = world.insertRandom(3, 1, 5);
    ASSERT_TRUE(result.ok()) << result.message;
    EXPECT_EQ(setKeys(world), (std::vector<Key>{1, 2, 3, 4, 5}));
}

TEST(WorldTest, RandomRangeExhaustedInsertsNothing) {
    World world;
    world.insert(2);
    const OpResult result = world.insertRandom(5, 1, 5);
    EXPECT_EQ(result.error, ErrorCode::RangeExhausted);
    EXPECT_TRUE(result.inserted.empty());
    EXPECT_EQ(setKeys(world), std::vector<Key>{2});
}

TEST(WorldTest, RandomRejectsBadArguments) {
    World world;
    EXPECT_EQ(world.insertRandom(0, 0, 10).error, ErrorCode::BadRequest);
    EXPECT_EQ(world.insertRandom(-1, 0, 10).error, ErrorCode::BadRequest);
    EXPECT_EQ(world.insertRandom(kMaxRandomCount + 1, 0, 1000000).error, ErrorCode::BadRequest);
    EXPECT_EQ(world.insertRandom(1, 10, 0).error, ErrorCode::BadRequest);
    EXPECT_TRUE(world.keys().empty());
}

TEST(WorldTest, RandomWorksOnFullInt64Range) {
    World world;
    const Key lo = std::numeric_limits<Key>::min();
    const Key hi = std::numeric_limits<Key>::max();
    const OpResult result = world.insertRandom(50, lo, hi);
    ASSERT_TRUE(result.ok()) << result.message;
    EXPECT_EQ(world.keys().size(), 50u);
}

TEST(WorldTest, RandomSingleKeyRangeAtInt64Edges) {
    World world;
    const Key hi = std::numeric_limits<Key>::max();
    const OpResult result = world.insertRandom(1, hi, hi);
    ASSERT_TRUE(result.ok()) << result.message;
    EXPECT_EQ(result.inserted, std::vector<Key>{hi});
    EXPECT_EQ(world.insertRandom(1, hi, hi).error, ErrorCode::RangeExhausted);
}

TEST(WorldTest, RandomIsReproducibleWithSameSeed) {
    WorldOptions options;
    options.keySeed = 123;
    World a(options);
    World b(options);
    const OpResult ra = a.insertRandom(30, 0, 999);
    const OpResult rb = b.insertRandom(30, 0, 999);
    EXPECT_EQ(ra.inserted, rb.inserted);

    const TreeSnapshot sa = a.activeSnapshot();
    const TreeSnapshot sb = b.activeSnapshot();
    ASSERT_EQ(sa.nodes.size(), sb.nodes.size());
    for (std::size_t i = 0; i < sa.nodes.size(); ++i) {
        EXPECT_EQ(sa.nodes[i].keys, sb.nodes[i].keys);
        EXPECT_EQ(sa.nodes[i].children, sb.nodes[i].children);
    }
}

TEST(WorldTest, RandomIsRoughlyUniform) {
    WorldOptions options;
    options.keySeed = 7;
    std::vector<int> hits(10, 0);
    for (int round = 0; round < 2000; ++round) {
        World world(options);
        options.keySeed += 1;
        world.insert(3);
        const OpResult result = world.insertRandom(1, 0, 9);
        ASSERT_TRUE(result.ok());
        ++hits[static_cast<std::size_t>(result.inserted[0])];
    }
    EXPECT_EQ(hits[3], 0);
    for (int key = 0; key < 10; ++key) {
        if (key != 3) {
            EXPECT_GT(hits[static_cast<std::size_t>(key)], 150) << "ключ " << key;
            EXPECT_LT(hits[static_cast<std::size_t>(key)], 300) << "ключ " << key;
        }
    }
}
