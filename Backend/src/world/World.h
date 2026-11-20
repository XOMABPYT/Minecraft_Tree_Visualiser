#pragma once

#include <cstdint>
#include <memory>
#include <random>
#include <set>
#include <string>
#include <vector>

#include "core/ITree.h"
#include "core/Key.h"
#include "core/TreeSnapshot.h"
#include "layout/Layout.h"
#include "world/TreeRegistry.h"

enum class ErrorCode {
    None,
    DuplicateKey,
    KeyNotFound,
    UnknownTree,
    RangeExhausted,
    BadRequest,
};

struct OpResult {
    ErrorCode error = ErrorCode::None;
    std::string message;
    // Для insertRandom: вставленные ключи в порядке вставки.
    std::vector<Key> inserted;

    bool ok() const { return error == ErrorCode::None; }
};

struct WorldOptions {
    // Seed ГСЧ случайных ключей.
    std::uint64_t keySeed = 1;
    // Seed, из которого получаются seed'ы деревьев (приоритеты treap).
    std::uint64_t treeSeed = 2;
};

// Верхняя граница count в insertRandom, чтобы один запрос не подвесил процесс.
constexpr std::int64_t kMaxRandomCount = 10000;

// Хранит все подключённые деревья; insert/erase применяются ко всем сразу,
// view лишь меняет, какое из них отображается.
class World {
public:
    explicit World(WorldOptions options = {});

    OpResult insert(Key key);
    OpResult insertRandom(std::int64_t count, Key min, Key max);
    OpResult erase(Key key);
    OpResult view(const std::string& treeId);
    void clear();

    // Пустая строка, если не подключено ни одного дерева.
    const std::string& activeTreeId() const;
    LayoutKind activeLayoutKind() const;
    TreeSnapshot activeSnapshot() const;

    const std::set<Key>& keys() const { return keys_; }
    std::vector<std::string> connectedTrees() const;

private:
    struct Slot {
        const TreeDescriptor* descriptor;
        std::unique_ptr<ITree> tree;
    };

    void createTrees();
    void insertIntoTrees(Key key);
    const Slot* activeSlot() const;

    WorldOptions options_;
    std::mt19937_64 keyRng_;
    std::vector<Slot> slots_;
    std::set<Key> keys_;
    std::string activeId_;
};

const char* errorCodeName(ErrorCode code);
