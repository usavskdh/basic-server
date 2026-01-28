// Standalone dedicated server for 1v1 combat
// Run this on Raspberry Pi or any Linux/Windows machine
// Then connect with 2 game clients

// Server doesn't need GLFW - no graphics, no input
#include "input_state.hpp"
#include "game_state.hpp"
#include "game_simulation.hpp"
#include "network_layer.hpp"

#include <iostream>
#include <chrono>
#include <thread>

constexpr uint16_t SERVER_PORT = 7777;
constexpr float TICK_RATE = 60.0f;  // 60 updates per second
constexpr float TICK_DURATION = 1.0f / TICK_RATE;

int main() {
    std::cout << "=== Combat Arena Server ===" << std::endl;
    std::cout << "Starting server on port " << SERVER_PORT << "..." << std::endl;

    ServerNetwork server;
    if (!server.Connect("", SERVER_PORT)) {
        std::cerr << "Failed to start server!" << std::endl;
        return 1;
    }
    std::cout << "Server started. Waiting for players..." << std::endl;

    GameSimulation sim;
    GameState gameState;
    gameState.ResetMatch();

    bool gameStarted = false;
    InputState playerInputs[2];

    // Setup callbacks
    server.OnPlayerJoined = [&](int playerIndex) {
        std::cout << "Player " << (playerIndex + 1) << " connected!" << std::endl;
    };

    server.OnGameStart = [&]() {
        std::cout << "Both players connected! Starting match..." << std::endl;
        gameStarted = true;
        gameState.ResetMatch();
    };

    server.OnInputReceived = [&](const InputState& input, int playerIndex) {
        playerInputs[playerIndex] = input;
    };

    server.OnDisconnected = [&](int playerIndex) {
        std::cout << "Player " << (playerIndex + 1) << " disconnected!" << std::endl;
        gameStarted = false;
    };

    // Server main loop
    auto lastTime = std::chrono::high_resolution_clock::now();
    float accumulator = 0.0f;

    std::cout << "\nServer running. Press Ctrl+C to stop.\n" << std::endl;

    while (true) {
        auto currentTime = std::chrono::high_resolution_clock::now();
        float deltaTime = std::chrono::duration<float>(currentTime - lastTime).count();
        lastTime = currentTime;

        // Process network events
        server.Update();

        if (gameStarted) {
            accumulator += deltaTime;

            // Fixed timestep simulation
            while (accumulator >= TICK_DURATION) {
                // Check for projectile spawns
                for (int i = 0; i < 2; i++) {
                    if (playerInputs[i].throwProjectile) {
                        GameSimulation::SpawnProjectile(gameState, i);
                    }
                }

                // Update game state
                gameState = sim.Update(gameState, playerInputs[0], playerInputs[1]);
                accumulator -= TICK_DURATION;

                // Check for round end
                bool roundOver = false;
                int winner = -1;

                for (int i = 0; i < 2; i++) {
                    if (!gameState.players[i].alive) {
                        roundOver = true;
                        winner = 1 - i;
                        break;
                    }
                }

                if (gameState.roundTimer <= 0.0f) {
                    roundOver = true;
                    if (gameState.players[0].hp > gameState.players[1].hp) {
                        winner = 0;
                    } else if (gameState.players[1].hp > gameState.players[0].hp) {
                        winner = 1;
                    }
                }

                if (roundOver) {
                    std::cout << "Round " << (int)gameState.currentRound << " over! ";
                    if (winner >= 0) {
                        std::cout << "Player " << (winner + 1) << " wins!" << std::endl;
                    } else {
                        std::cout << "Draw!" << std::endl;
                    }

                    // Check for match end
                    for (int i = 0; i < 2; i++) {
                        if (gameState.players[i].roundWins >= 2) {
                            std::cout << "=== MATCH OVER! Player " << (i + 1) << " wins the match! ===" << std::endl;
                            gameStarted = false;
                            gameState.ResetMatch();
                            break;
                        }
                    }

                    if (gameStarted) {
                        gameState.currentRound++;
                        gameState.ResetRound();
                    }
                }
            }

            // Send game state to all clients
            server.SendGameState(gameState);

            // Print status every few seconds
            static int frameCounter = 0;
            if (++frameCounter % 180 == 0) {  // Every 3 seconds at 60fps
                std::cout << "Frame " << gameState.frameNumber
                          << " | P1: " << gameState.players[0].hp << " HP"
                          << " | P2: " << gameState.players[1].hp << " HP"
                          << " | Projectiles: " << gameState.projectiles.size()
                          << " | Timer: " << (int)gameState.roundTimer << "s"
                          << std::endl;
            }
        }

        // Sleep to maintain tick rate
        auto frameEnd = std::chrono::high_resolution_clock::now();
        float frameTime = std::chrono::duration<float>(frameEnd - currentTime).count();
        if (frameTime < TICK_DURATION) {
            std::this_thread::sleep_for(
                std::chrono::milliseconds(static_cast<int>((TICK_DURATION - frameTime) * 1000))
            );
        }
    }

    return 0;
}
