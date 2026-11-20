#include <cstdint>
#include <cstring>
#include <iostream>
#include <random>
#include <string>

#include "protocol/Protocol.h"
#include "world/World.h"

namespace {

void printUsage() {
    std::cerr << "Использование: tree_backend [--seed <uint64>]\n"
                 "Читает запросы из stdin (одна строка JSON на запрос), ответы пишет в stdout."
              << std::endl;
}

}  // namespace

int main(int argc, char** argv) {
    std::uint64_t seed = (static_cast<std::uint64_t>(std::random_device{}()) << 32) ^
                         std::random_device{}();
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--seed") == 0 && i + 1 < argc) {
            try {
                seed = std::stoull(argv[++i]);
            } catch (const std::exception&) {
                std::cerr << "[error] некорректный seed: " << argv[i] << std::endl;
                return 2;
            }
        } else {
            printUsage();
            return 2;
        }
    }

    WorldOptions options;
    options.keySeed = seed;
    options.treeSeed = seed ^ 0x5DEECE66DULL;
    World world(options);

    std::cerr << "[info] backend запущен, seed = " << seed << ", деревья:";
    for (const auto& id : world.connectedTrees()) {
        std::cerr << ' ' << id;
    }
    std::cerr << std::endl;

    std::string line;
    while (std::getline(std::cin, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.find_first_not_of(" \t") == std::string::npos) {
            continue;
        }
        std::cout << handleRequestLine(world, line) << '\n' << std::flush;
    }
    std::cerr << "[info] stdin закрыт, backend завершает работу" << std::endl;
    return 0;
}
