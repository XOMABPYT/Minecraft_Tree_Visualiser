#pragma once

#include <utility>
#include <random>
#include <cstdint>

class Treap {
  public:
    explicit Treap (std::uint64_t seed = 1337) : rng(seed) {}
    ~Treap () { destroy(root); }

    bool insert(std::int64_t x);
    bool erase(std::int64_t x);

    Treap(const Treap&) = delete;
    Treap& operator=(const Treap&) = delete;

  private:
    struct Node {
        Node* left = nullptr;
        Node* right = nullptr;
        std::int64_t x;
        std::uint64_t y;
        int sz = 1;
        std::int64_t mn;
        std::int64_t mx;
        int h = 1;
        
        Node(std::int64_t x, std::uint64_t y) : x(x), y(y), mn(x), mx(x) {}
    };

    Node* root = nullptr;
    std::mt19937_64 rng;

    static int get_size(Node* t);
    static int get_height(Node* t);
    static std::int64_t get_min(Node* t);
    static std::int64_t get_max(Node* t);
    static void update(Node* t);
    static Node* merge(Node* t1, Node* t2);
    static std::pair<Node*, Node*> split(Node* t, std::int64_t k);
    static void destroy(Node* t);
};