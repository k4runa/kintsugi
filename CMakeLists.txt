cmake_minimum_required(VERSION 3.10)

project(Kintsugi CXX)

# C++20 for starts_with() and structured bindings in range-for, both used all over
# key_map and store.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# One target, sources listed by hand instead of a glob so a new file has to be added
# here on purpose. Bottom of the stack first, roughly: disk, cache, index, then the
# layers on top. The tests under tests/ are built separately, they are not part of this.
add_executable(kintsugi
    src/btree_node.cpp
    src/buffer_pool.cpp
    src/disk_manager.cpp
    src/wal_manager.cpp
    src/serializer.cpp
    src/key_map.cpp
    src/store.cpp

    src/main.cpp
)

target_include_directories(kintsugi PRIVATE include)
