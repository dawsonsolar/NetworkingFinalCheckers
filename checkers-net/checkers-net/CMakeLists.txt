cmake_minimum_required(VERSION 3.20)
project(CheckersNet LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Threads (pthreads on Linux/WSL) ───────────────────────────────────────
find_package(Threads REQUIRED)

# ── Shared game logic (static lib so both targets link against it) ─────────
add_library(checkers_logic STATIC
    shared/CheckersLogic.cpp
    shared/CheckersLogic.h
    shared/Protocol.h
)
target_include_directories(checkers_logic PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/shared)

# ── Server ─────────────────────────────────────────────────────────────────
add_executable(checkers-server
    server/main.cpp
    server/GameServer.cpp
    server/GameServer.h
    server/GameRoom.cpp
    server/GameRoom.h
    server/Session.h
)
target_include_directories(checkers-server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/server
)
target_link_libraries(checkers-server PRIVATE
    checkers_logic
    Threads::Threads
)

# ── Client ─────────────────────────────────────────────────────────────────
add_executable(checkers-client
    client/main.cpp
)
target_include_directories(checkers-client PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/client
)
target_link_libraries(checkers-client PRIVATE
    checkers_logic
)
