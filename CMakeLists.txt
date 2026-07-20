cmake_minimum_required(VERSION 3.10)

project(Kintsugi CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(kintsugi 
    src/btree_node.cpp
    src/buffer_pool.cpp
    src/disk_manager.cpp
    src/wal_manager.cpp

    src/main.cpp
)

target_include_directories(kintsugi PRIVATE include)
