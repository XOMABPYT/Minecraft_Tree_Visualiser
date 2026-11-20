#pragma once

#include "Key.h"
#include "TreeSnapshot.h"

class ITree {
public:
    virtual ~ITree() = default;

    // false, если ключ уже есть; дерево при этом не меняется.
    virtual bool insert(Key key) = 0;
    // false, если ключа нет; дерево при этом не меняется.
    virtual bool erase(Key key) = 0;
    // Не должен менять дерево (важно для splay).
    virtual TreeSnapshot snapshot() const = 0;
};
