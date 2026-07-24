cmake_minimum_required(VERSION 3.16)
project(console_pet CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Wide-character ncurses is required for Unicode (emoji / box-drawing) output.
find_package(PkgConfig REQUIRED)
pkg_check_modules(NCURSESW REQUIRED IMPORTED_TARGET ncursesw)

# Threads power the background LLM "brain".
find_package(Threads REQUIRED)

# Core library: all game logic + rendering, shared by the server and client.
add_library(console_pet_core STATIC
    src/text.cpp
    src/pet_stats.cpp
    src/mood.cpp
    src/pet_art.cpp
    src/pet.cpp
    src/save.cpp
    src/shop.cpp
    src/config.cpp
    src/llm.cpp
    src/brain.cpp
    src/proto.cpp
    src/net.cpp
    src/ui.cpp
)
target_include_directories(console_pet_core
    PUBLIC include third_party
)
target_link_libraries(console_pet_core PUBLIC PkgConfig::NCURSESW Threads::Threads)
target_compile_options(console_pet_core PRIVATE -Wall -Wextra)

# Server: owns the pet/simulation/shop/LLM and serves clients over TCP.
add_executable(console_pet_server src/server_main.cpp)
target_link_libraries(console_pet_server PRIVATE console_pet_core)

# Client: thin terminal UI that controls the server and renders its state.
add_executable(console_pet_client src/client_main.cpp)
target_link_libraries(console_pet_client PRIVATE console_pet_core)
