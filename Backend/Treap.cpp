#include <Treap.h>
#include <utility>

Treap* Treap::merge(Treap* t1, Treap* t2) {
    if (!t1) return t2;
    if (!t2) return t1;

    if (t1->y > t2->y) {
        t1->right = merge(t1->right, t2);
        return t1;
    }
    t2->left = merge(t1, t2->left);
    return t2;
}

int Treap::get_size(Treap* t) {
    if (!t) return 0;
    return t->sz;
}

int Treap::get_height(Treap* t) {
    if (!t) return 0;
    return t->h;
}

std::pair<Treap*, Treap*> Treap::split(Treap* t, int k) {
    if (!t) return {0, 0};
    if (get_size(t->left) + 1 <= k) {
        auto q = split(t->right, k - get_size(t->left) - 1);
        t->right = q.first;
        return {t, q.second};
    }
    auto q = split(t->left, k);
    t->left = q.second;
    return {q.first, t};
}