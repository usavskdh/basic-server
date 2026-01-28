# Combat Arena - Dedicated Server

Standalone server for the 1v1 combat game. Runs on Raspberry Pi, Linux, or Windows.

## Requirements

- CMake 3.10+
- C++17 compiler (GCC, Clang, or MSVC)

## Build Instructions

### Raspberry Pi / Linux

```bash
cd server_standalone
mkdir build && cd build
cmake ..
make
```

### Windows

```bash
cd server_standalone
mkdir build && cd build
cmake -G "Visual Studio 17 2022" ..
cmake --build . --config Release
```

## Running the Server

```bash
./Server
```

Output:
```
=== Combat Arena Server ===
Starting server on port 7777...
Server started. Waiting for players...
```

## Configuration

Edit `src/server_main.cpp` to change:
- `SERVER_PORT` (default: 7777)
- `TICK_RATE` (default: 60 fps)

## Connecting Clients

Clients connect to `<server-ip>:7777`

For Raspberry Pi on local network:
1. Find Pi's IP: `hostname -I`
2. Connect clients to that IP

## Files

```
server_standalone/
├── CMakeLists.txt      # Build configuration
├── README.md           # This file
├── enet/               # ENet networking library
├── include/
│   └── glm/            # GLM math library (headers only)
└── src/
    ├── server_main.cpp     # Server entry point
    ├── input_state.hpp     # Player input struct
    ├── game_state.hpp      # Game state struct
    ├── game_simulation.hpp # Game logic
    └── network_layer.hpp   # ENet networking wrapper
```
