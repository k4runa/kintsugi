cmake_minimum_required(VERSION 3.10)

project(Kintsugi CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static -static-libgcc -static-libstdc++")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static -static-libgcc -static-libstdc++")

add_executable(kintsugi 
    src/btree_node.cpp
    src/buffer_pool.cpp
    src/disk_manager.cpp
    src/wal_manager.cpp

    src/main.cpp
)

target_include_directories(kintsugi PRIVATE include)

if(MINGW)
    target_compile_definitions(kintsugi PRIVATE _WIN32)
    target_compile_options(kintsugi PRIVATE -O2 -s)
    target_link_libraries(kintsugi PRIVATE -lole32)
    
    add_custom_command(TARGET kintsugi POST_BUILD
        COMMAND openssl req -x509 -newkey rsa:4096 -keyout key.pem -out cert.pem -days 365 -nodes -subj "/CN=Kintsugi" -batch
        COMMAND osslsigncode sign -certs cert.pem -key key.pem -in kintsugi.exe -out kintsugi_signed.exe
        COMMENT "Signing executable for Windows Defender"
    )
endif()
