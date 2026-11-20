cmake_minimum_required(VERSION 3.20)

project(TreeVisualiserBackend LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)

FetchContent_Declare(
    nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(nlohmann_json)

# Деревья автора подключаются сюда по мере готовности
# (и регистрируются в src/world/TreeRegistry.cpp).
set(AUTHOR_TREE_SOURCES
    Treap.cpp
)

add_library(tree_core STATIC
    ${AUTHOR_TREE_SOURCES}
    src/layout/Layout.cpp
    src/protocol/Protocol.cpp
    src/world/TreeRegistry.cpp
    src/world/World.cpp
)
target_include_directories(tree_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(tree_core PUBLIC nlohmann_json::nlohmann_json)
target_compile_options(tree_core PRIVATE -Wall -Wextra)

add_executable(tree_backend src/main.cpp)
target_link_libraries(tree_backend PRIVATE tree_core)
target_compile_options(tree_backend PRIVATE -Wall -Wextra)

include(CTest)
if(BUILD_TESTING)
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.tar.gz
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    add_executable(backend_tests
        tests/LayoutTest.cpp
        tests/ProtocolTest.cpp
        tests/TreapTest.cpp
        tests/WorldTest.cpp
    )
    target_link_libraries(backend_tests PRIVATE tree_core GTest::gtest_main)
    target_compile_options(backend_tests PRIVATE -Wall -Wextra)

    include(GoogleTest)
    gtest_discover_tests(backend_tests)
endif()
