#ifndef GAME_SIMULATION_H
#define GAME_SIMULATION_H

#include "game_state.hpp"
#include "input_state.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

#include <cmath>
#include <algorithm>

// GameSimulation handles all deterministic game logic.
// CRITICAL: This must be 100% deterministic!
// Same inputs + same state = same output (for rollback netcode)
//
// Rules:
// - NO random() or rand() - use seeded PRNG if needed
// - NO system time - use frame count
// - NO floating point optimizations that vary by platform (-ffast-math)

class GameSimulation {
public:
    // Fixed timestep (1/60 second)
    static constexpr float FIXED_DT = 1.0f / 60.0f;

    // Arena bounds
    static constexpr float ARENA_HALF_SIZE = 20.0f;

    // Projectile collision radius
    static constexpr float PROJECTILE_RADIUS = 0.5f;
    static constexpr float PLAYER_RADIUS = 1.0f;

    // Main update function - advances game by one frame
    GameState Update(const GameState& current, const InputState& p1Input, const InputState& p2Input) {
        GameState next = current;
        next.frameNumber++;

        // Update round timer
        next.roundTimer -= FIXED_DT;
        if (next.roundTimer < 0.0f) {
            next.roundTimer = 0.0f;
        }

        // Process each player
        const InputState* inputs[2] = { &p1Input, &p2Input };
        for (int i = 0; i < 2; i++) {
            UpdatePlayer(next.players[i], *inputs[i], i);
        }

        // Update projectiles
        UpdateProjectiles(next);

        // Check projectile-player collisions
        CheckCollisions(next);

        // Check win conditions
        CheckWinConditions(next);

        return next;
    }

    // For rollback: save current state
    GameState SaveState(const GameState& state) const {
        return state;  // GameState is copyable
    }

    // For rollback: restore to previous state
    void RestoreState(GameState& target, const GameState& saved) const {
        target = saved;
    }

private:
    // Track previous frame's throw button state to detect press (not hold)
    bool prevThrowPressed[2] = { false, false };

    void UpdatePlayer(PlayerState& player, const InputState& input, int playerIndex) {
        if (!player.alive) return;

        // Movement
        glm::vec3 moveDir(input.moveX, 0.0f, input.moveY);
        float moveLen = glm::length(moveDir);

        if (moveLen > 0.01f) {
            // Normalize and apply speed
            moveDir = glm::normalize(moveDir) * GameConstants::PLAYER_SPEED * FIXED_DT;
            player.position += moveDir;

            // Update facing angle based on movement direction
            player.facingAngle = glm::degrees(std::atan2(input.moveX, -input.moveY));
        }

        // Clamp to arena bounds
        player.position.x = std::clamp(player.position.x, -ARENA_HALF_SIZE, ARENA_HALF_SIZE);
        player.position.z = std::clamp(player.position.z, -ARENA_HALF_SIZE, ARENA_HALF_SIZE);

        // Cooldown
        if (player.projectileCooldown > 0.0f) {
            player.projectileCooldown -= FIXED_DT;
            if (player.projectileCooldown < 0.0f) {
                player.projectileCooldown = 0.0f;
            }
        }
    }

    void UpdateProjectiles(GameState& state) {
        // Spawn new projectiles (check for button press, not hold)
        for (int i = 0; i < 2; i++) {
            // Note: We can't track prevThrowPressed in a stateless way
            // For now, allow shooting if cooldown is 0 and button pressed
            // This means holding the button will fire at cooldown rate
        }

        // Move existing projectiles
        for (auto& proj : state.projectiles) {
            if (!proj.active) continue;

            proj.position += proj.velocity * FIXED_DT;

            // Deactivate if out of bounds
            if (std::abs(proj.position.x) > ARENA_HALF_SIZE + 5.0f ||
                std::abs(proj.position.z) > ARENA_HALF_SIZE + 5.0f) {
                proj.active = false;
            }
        }

        // Remove inactive projectiles
        state.projectiles.erase(
            std::remove_if(state.projectiles.begin(), state.projectiles.end(),
                [](const ProjectileState& p) { return !p.active; }),
            state.projectiles.end()
        );
    }

    void CheckCollisions(GameState& state) {
        for (auto& proj : state.projectiles) {
            if (!proj.active) continue;

            // Check collision with each player
            for (int i = 0; i < 2; i++) {
                // Don't hit the player who shot it
                if (proj.ownerID == i) continue;
                if (!state.players[i].alive) continue;

                // Simple sphere collision
                glm::vec3 diff = proj.position - state.players[i].position;
                float dist = glm::length(diff);

                if (dist < PROJECTILE_RADIUS + PLAYER_RADIUS) {
                    // Hit!
                    state.players[i].hp -= proj.damage;
                    proj.active = false;

                    if (state.players[i].hp <= 0.0f) {
                        state.players[i].hp = 0.0f;
                        state.players[i].alive = false;
                    }
                    break;
                }
            }
        }
    }

    void CheckWinConditions(GameState& state) {
        // Check if a player died
        int deadPlayer = -1;
        for (int i = 0; i < 2; i++) {
            if (!state.players[i].alive) {
                deadPlayer = i;
                break;
            }
        }

        // Check timeout
        bool timeout = (state.roundTimer <= 0.0f);

        if (deadPlayer >= 0) {
            // Other player wins the round
            int winner = 1 - deadPlayer;
            state.players[winner].roundWins++;
        } else if (timeout) {
            // Higher HP wins
            if (state.players[0].hp > state.players[1].hp) {
                state.players[0].roundWins++;
            } else if (state.players[1].hp > state.players[0].hp) {
                state.players[1].roundWins++;
            }
            // Draw: nobody gets a point (could change this)
        }

        // Note: Round/match transitions should be handled by game flow controller
        // This just updates the win counts
    }

public:
    // Helper to spawn a projectile (call this from game loop when input detected)
    static void SpawnProjectile(GameState& state, int playerIndex) {
        PlayerState& player = state.players[playerIndex];

        if (player.projectileCooldown > 0.0f || !player.alive) {
            return;  // Can't shoot yet
        }

        ProjectileState proj;
        proj.ownerID = static_cast<uint8_t>(playerIndex);
        proj.damage = GameConstants::PROJECTILE_DAMAGE;

        // Spawn slightly in front of player
        float angleRad = glm::radians(player.facingAngle);
        glm::vec3 dir(std::sin(angleRad), 0.0f, -std::cos(angleRad));

        proj.position = player.position + dir * (PLAYER_RADIUS + PROJECTILE_RADIUS + 0.1f);
        proj.velocity = dir * GameConstants::PROJECTILE_SPEED;
        proj.active = true;

        state.projectiles.push_back(proj);
        player.projectileCooldown = GameConstants::PROJECTILE_COOLDOWN;
    }

    // Check if player can shoot (for UI feedback)
    static bool CanShoot(const PlayerState& player) {
        return player.alive && player.projectileCooldown <= 0.0f;
    }
};

#endif
