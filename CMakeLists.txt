# Combat Arena - Dedicated Server
# Runs on Raspberry Pi, Linux, or Windows
#
# Build instructions:
#   mkdir build && cd build
#   cmake ..
#   make
#   ./Server
#
# The server will listen on port 7777 by default.

cmake_minimum_required(VERSION 3.10)
project(CombatServer LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ENet networking library
add_subdirectory(enet)

# Server executable
add_executable(Server
    src/server_main.cpp
)

target_include_directories(Server PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/enet/include
)

target_link_libraries(Server PRIVATE enet)

# Platform-specific libraries
if(WIN32)
    target_link_libraries(Server PRIVATE ws2_32 winmm)
endif()

# On Linux/Pi, may need pthread
if(UNIX AND NOT APPLE)
    find_package(Threads REQUIRED)
    target_link_libraries(Server PRIVATE Threads::Threads)
endif()

# Print build info
message(STATUS "Building Combat Arena Server")
message(STATUS "  Platform: ${CMAKE_SYSTEM_NAME}")
message(STATUS "  Architecture: ${CMAKE_SYSTEM_PROCESSOR}")
