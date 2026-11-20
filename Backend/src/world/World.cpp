#include "world/World.h"

#include <iostream>
#include <iterator>
#include <string>

namespace {

std::uint64_t splitMix64(std::uint64_t value) {
    value += 0x9E3779B97F4A7C15ULL;
    value = (value ^ (value >> 30)) * 0xBF58476D1CE4E5B9ULL;
    value = (value ^ (value >> 27)) * 0x94D049BB133111EBULL;
    return value ^ (value >> 31);
}

std::uint64_t offsetFrom(Key min, Key key) {
    return static_cast<std::uint64_t>(key) - static_cast<std::uint64_t>(min);
}

}  // namespace

World::World(WorldOptions options) : options_(options), keyRng_(options.keySeed) {
    createTrees();
}

void World::createTrees() {
    slots_.clear();
    const auto& descriptors = allTrees();
    for (std::size_t i = 0; i < descriptors.size(); ++i) {
        const std::uint64_t seed = splitMix64(options_.treeSeed + i);
        auto tree = createTree(descriptors[i].id, seed);
        if (tree) {
            slots_.push_back({&descriptors[i], std::move(tree)});
        }
    }
    if (activeSlot() == nullptr) {
        activeId_ = slots_.empty() ? std::string() : slots_.front().descriptor->id;
    }
}

const World::Slot* World::activeSlot() const {
    for (const auto& slot : slots_) {
        if (slot.descriptor->id == activeId_) {
            return &slot;
        }
    }
    return nullptr;
}

void World::insertIntoTrees(Key key) {
    for (auto& slot : slots_) {
        if (!slot.tree->insert(key)) {
            std::cerr << "[warn] дерево " << slot.descriptor->id << " не вставило новый ключ " << key
                      << std::endl;
        }
    }
}

OpResult World::insert(Key key) {
    OpResult result;
    if (keys_.count(key) != 0) {
        result.error = ErrorCode::DuplicateKey;
        result.message = "Key " + std::to_string(key) + " already exists";
        return result;
    }
    keys_.insert(key);
    insertIntoTrees(key);
    return result;
}

OpResult World::insertRandom(std::int64_t count, Key min, Key max) {
    OpResult result;
    if (count < 1 || count > kMaxRandomCount) {
        result.error = ErrorCode::BadRequest;
        result.message = "count must be in [1, " + std::to_string(kMaxRandomCount) + "]";
        return result;
    }
    if (min > max) {
        result.error = ErrorCode::BadRequest;
        result.message = "min must not exceed max";
        return result;
    }

    const auto first = keys_.lower_bound(min);
    const auto last = keys_.upper_bound(max);
    std::uint64_t occupied = static_cast<std::uint64_t>(std::distance(first, last));
    // Размер диапазона минус один: сам размер может не поместиться в uint64.
    const std::uint64_t span = offsetFrom(min, max);
    const std::uint64_t requested = static_cast<std::uint64_t>(count);
    if (occupied > span || requested - 1 > span - occupied) {
        const std::uint64_t freeKeys = occupied > span ? 0 : span - occupied + 1;
        result.error = ErrorCode::RangeExhausted;
        result.message = "Only " + std::to_string(freeKeys) + " free keys in [" +
                         std::to_string(min) + ", " + std::to_string(max) + "], requested " +
                         std::to_string(count);
        return result;
    }

    for (std::int64_t i = 0; i < count; ++i) {
        // Равномерно выбираем номер среди свободных ключей, затем сдвигаем его
        // через занятые ключи диапазона по возрастанию.
        std::uniform_int_distribution<std::uint64_t> pick(0, span - occupied);
        std::uint64_t offset = pick(keyRng_);
        for (auto it = keys_.lower_bound(min); it != keys_.end() && *it <= max; ++it) {
            if (offsetFrom(min, *it) > offset) {
                break;
            }
            ++offset;
        }
        const Key key = static_cast<Key>(static_cast<std::uint64_t>(min) + offset);
        keys_.insert(key);
        insertIntoTrees(key);
        result.inserted.push_back(key);
        ++occupied;
    }
    return result;
}

OpResult World::erase(Key key) {
    OpResult result;
    if (keys_.erase(key) == 0) {
        result.error = ErrorCode::KeyNotFound;
        result.message = "Key " + std::to_string(key) + " not found";
        return result;
    }
    for (auto& slot : slots_) {
        if (!slot.tree->erase(key)) {
            std::cerr << "[warn] дерево " << slot.descriptor->id << " не удалило существующий ключ "
                      << key << std::endl;
        }
    }
    return result;
}

OpResult World::view(const std::string& treeId) {
    OpResult result;
    if (findTree(treeId) == nullptr) {
        result.error = ErrorCode::UnknownTree;
        result.message = "Unknown tree '" + treeId + "'";
        return result;
    }
    for (const auto& slot : slots_) {
        if (slot.descriptor->id == treeId) {
            activeId_ = treeId;
            return result;
        }
    }
    result.error = ErrorCode::UnknownTree;
    result.message = "Tree '" + treeId + "' is not implemented yet";
    return result;
}

void World::clear() {
    keys_.clear();
    createTrees();
}

const std::string& World::activeTreeId() const {
    return activeId_;
}

LayoutKind World::activeLayoutKind() const {
    const Slot* slot = activeSlot();
    return slot ? slot->descriptor->layout : LayoutKind::Binary;
}

TreeSnapshot World::activeSnapshot() const {
    const Slot* slot = activeSlot();
    return slot ? slot->tree->snapshot() : TreeSnapshot{};
}

std::vector<std::string> World::connectedTrees() const {
    std::vector<std::string> ids;
    for (const auto& slot : slots_) {
        ids.push_back(slot.descriptor->id);
    }
    return ids;
}

const char* errorCodeName(ErrorCode code) {
    switch (code) {
        case ErrorCode::None:
            return "";
        case ErrorCode::DuplicateKey:
            return "duplicate_key";
        case ErrorCode::KeyNotFound:
            return "key_not_found";
        case ErrorCode::UnknownTree:
            return "unknown_tree";
        case ErrorCode::RangeExhausted:
            return "range_exhausted";
        case ErrorCode::BadRequest:
            return "bad_request";
    }
    return "bad_request";
}
