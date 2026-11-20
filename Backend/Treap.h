#include <random>
#include <utility>

class Treap {
    Treap* left;
    Treap* right;
    
    int x;
    int y = rand();
    int sz = 1;
    int h;

    Treap* merge(Treap* t1, Treap* t2) {}

    int get_size(Treap* t) {}
    int get_height(Treap* t) {}

    std::pair<Treap*, Treap*> split(Treap* t, int k) {}

};