#include "Treap.h"
#include <utility>
#include <algorithm>

int Treap::get_size(Node* t) {
    if (!t) return 0;
    return t->sz;
}

int Treap::get_height(Node* t) {
    if (!t) return 0;
    return t->h;
}

int64_t Treap::get_min(Node* t) {
    if (!t) return 1e18;
    return t->mn;
}

int64_t Treap::get_max(Node* t) {
    if (!t) return -1e18;
    return t->mx;
}

void Treap::update(Node* t) {
    if (!t) return;
    t->sz = get_size(t->left) + get_size(t->right) + 1;
    t->mn = std::min({t->x, get_min(t->left), get_min(t->right)});
    t->mx = std::max({t->x, get_max(t->left), get_max(t->right)});
    t->h = std::max(get_height(t->left) + 1, get_height(t->right) + 1);
}

Treap::Node* Treap::merge(Node* t1, Node* t2) {
    if (!t1) return t2;
    if (!t2) return t1;

    if (t1->y > t2->y) {
        t1->right = merge(t1->right, t2);
        update(t1);
        return t1;
    }
    t2->left = merge(t1, t2->left);
    update(t2);
    return t2;
}

std::pair<Treap::Node*, Treap::Node*> Treap::split(Node* t, std::int64_t k) {
    if (!t) return {0, 0};
    if (t->x < k) {
        auto q = split(t->right, k);
        t->right = q.first;
        update(t);
        return {t, q.second};
    }
    auto q = split(t->left, k);
    t->left = q.second;
    update(t);
    return {q.first, t};
}

bool Treap::insert(std::int64_t x) {
    auto q = split(root, x);
    if (q.second) {
        if (get_min(q.second) == x) {
            root = merge(q.first, q.second);
            return false;
        }
    }
    q.second = merge(new Node(x, rng()), q.second);
    root = merge(q.first, q.second);
    return true;
}

bool Treap::erase(int64_t x) {
    auto q = split(root, x);
    auto q2 = split(q.second, x + 1);
    if (!q2.first) {
        root = merge(q.first, q.second);
        return false;
    }
    delete q2.first;
    root = merge(q.first, q2.second);
    return true;
}

void Treap::destroy(Node* t) {
    if (!t) return;
    destroy(t->left);
    destroy(t->right);
    delete t;
}