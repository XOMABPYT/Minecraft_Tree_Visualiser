#include <gtest/gtest.h>

#include <set>
#include <string>

#include <nlohmann/json.hpp>

#include "protocol/Protocol.h"

using nlohmann::json;

namespace {

json call(World& world, const std::string& line) {
    const std::string response = handleRequestLine(world, line);
    EXPECT_EQ(response.find('\n'), std::string::npos) << "ответ должен быть одной строкой";
    return json::parse(response);
}

void expectError(const json& response, const std::string& code) {
    EXPECT_EQ(response["ok"], false);
    EXPECT_EQ(response["error"], code);
    EXPECT_TRUE(response["message"].is_string());
    EXPECT_FALSE(response.contains("nodes"));
}

}  // namespace

TEST(ProtocolTest, InsertReturnsLayout) {
    World world;
    const json r = call(world, R"({"id": 1, "cmd": "insert", "key": 42})");
    EXPECT_EQ(r["id"], 1);
    EXPECT_EQ(r["ok"], true);
    EXPECT_EQ(r["tree"], "treap");
    ASSERT_EQ(r["nodes"].size(), 1u);
    const json& node = r["nodes"][0];
    EXPECT_TRUE(node["id"].is_number_integer());
    EXPECT_EQ(node["keys"], json::array({42}));
    EXPECT_EQ(node["x"], 0);
    EXPECT_EQ(node["depth"], 0);
    EXPECT_EQ(node["color"], "white");
    EXPECT_TRUE(node["label"].is_array());
    EXPECT_EQ(node["label"][0], "42");
    EXPECT_TRUE(node["info"].is_object());
    EXPECT_TRUE(r["edges"].empty());
}

TEST(ProtocolTest, EdgesReferenceNodesOfResponse) {
    World world;
    for (int key : {5, 3, 8, 1, 4, 7, 9}) {
        call(world, R"({"id": 0, "cmd": "insert", "key": )" + std::to_string(key) + "}");
    }
    const json r = call(world, R"({"id": 2, "cmd": "state"})");
    ASSERT_EQ(r["nodes"].size(), 7u);
    EXPECT_EQ(r["edges"].size(), 6u);

    std::set<int> ids;
    for (const auto& node : r["nodes"]) {
        EXPECT_TRUE(ids.insert(node["id"].get<int>()).second);
    }
    for (const auto& edge : r["edges"]) {
        EXPECT_EQ(ids.count(edge["from"].get<int>()), 1u);
        EXPECT_EQ(ids.count(edge["to"].get<int>()), 1u);
        const int slot = edge["slot"];
        EXPECT_TRUE(slot == 0 || slot == 1);
    }
}

TEST(ProtocolTest, DuplicateKeyError) {
    World world;
    call(world, R"({"id": 1, "cmd": "insert", "key": 42})");
    const json r = call(world, R"({"id": 2, "cmd": "insert", "key": 42})");
    EXPECT_EQ(r["id"], 2);
    expectError(r, "duplicate_key");
}

TEST(ProtocolTest, DeleteKey) {
    World world;
    call(world, R"({"id": 1, "cmd": "insert", "key": 42})");
    const json ok = call(world, R"({"id": 2, "cmd": "delete", "key": 42})");
    EXPECT_EQ(ok["ok"], true);
    EXPECT_TRUE(ok["nodes"].empty());
    expectError(call(world, R"({"id": 3, "cmd": "delete", "key": 42})"), "key_not_found");
}

TEST(ProtocolTest, RandomReturnsInsertedKeys) {
    World world;
    const json r = call(world, R"({"id": 1, "cmd": "random", "count": 10, "min": 0, "max": 999})");
    EXPECT_EQ(r["ok"], true);
    ASSERT_TRUE(r["inserted"].is_array());
    EXPECT_EQ(r["inserted"].size(), 10u);
    EXPECT_EQ(r["nodes"].size(), 10u);
}

TEST(ProtocolTest, RandomRangeExhausted) {
    World world;
    const json r = call(world, R"({"id": 1, "cmd": "random", "count": 5, "min": 0, "max": 3})");
    expectError(r, "range_exhausted");
    EXPECT_FALSE(r.contains("inserted"));
    EXPECT_TRUE(call(world, R"({"id": 2, "cmd": "state"})")["nodes"].empty());
}

TEST(ProtocolTest, ViewErrors) {
    World world;
    expectError(call(world, R"({"id": 1, "cmd": "view", "tree": "oak"})"), "unknown_tree");
    expectError(call(world, R"({"id": 2, "cmd": "view", "tree": "avl"})"), "unknown_tree");
    const json r = call(world, R"({"id": 3, "cmd": "view", "tree": "treap"})");
    EXPECT_EQ(r["ok"], true);
    EXPECT_EQ(r["tree"], "treap");
}

TEST(ProtocolTest, ClearEmptiesTree) {
    World world;
    call(world, R"({"id": 1, "cmd": "insert", "key": 1})");
    const json r = call(world, R"({"id": 2, "cmd": "clear"})");
    EXPECT_EQ(r["ok"], true);
    EXPECT_TRUE(r["nodes"].empty());
    EXPECT_TRUE(r["edges"].empty());
}

TEST(ProtocolTest, MalformedRequests) {
    World world;
    const json broken = call(world, "{not json");
    expectError(broken, "bad_request");
    EXPECT_TRUE(broken["id"].is_null());

    expectError(call(world, "[1, 2]"), "bad_request");
    expectError(call(world, R"({"id": 1})"), "bad_request");
    expectError(call(world, R"({"id": 1, "cmd": "fly"})"), "bad_request");
    expectError(call(world, R"({"id": 1, "cmd": "insert"})"), "bad_request");
    expectError(call(world, R"({"id": 1, "cmd": "insert", "key": "42"})"), "bad_request");
    expectError(call(world, R"({"id": 1, "cmd": "insert", "key": 4.5})"), "bad_request");
    expectError(call(world, R"({"id": 1, "cmd": "insert", "key": 9223372036854775808})"),
                "bad_request");
    expectError(call(world, R"({"id": 1, "cmd": "random", "count": 3})"), "bad_request");
    expectError(call(world, R"({"id": 1, "cmd": "random", "count": 0, "min": 0, "max": 9})"),
                "bad_request");
    expectError(call(world, R"({"id": 1, "cmd": "view", "tree": 5})"), "bad_request");

    EXPECT_TRUE(call(world, R"({"id": 2, "cmd": "state"})")["nodes"].empty());
}

TEST(ProtocolTest, Int64KeyBounds) {
    World world;
    const json lo = call(world, R"({"id": 1, "cmd": "insert", "key": -9223372036854775808})");
    EXPECT_EQ(lo["ok"], true);
    const json hi = call(world, R"({"id": 2, "cmd": "insert", "key": 9223372036854775807})");
    EXPECT_EQ(hi["ok"], true);
    EXPECT_EQ(hi["nodes"].size(), 2u);
}

TEST(ProtocolTest, EchoesRequestId) {
    World world;
    EXPECT_EQ(call(world, R"({"id": "abc", "cmd": "state"})")["id"], "abc");
    EXPECT_TRUE(call(world, R"({"cmd": "state"})")["id"].is_null());
}
