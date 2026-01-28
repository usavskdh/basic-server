#ifndef GAME_STATE_H
#define GAME_STATE_H

#include <cstdint>
#include <cstring>
#include <vector>

#include <glm/glm.hpp>

// Default game constants (tweak as needed)
namespace GameConstants {
    constexpr float STARTING_HP = 100.0f;
    constexpr float PROJECTILE_DAMAGE = 10.0f;
    constexpr float PROJECTILE_SPEED = 20.0f;
    constexpr float PROJECTILE_COOLDOWN = 0.5f;  // seconds between shots
    constexpr float PLAYER_SPEED = 5.0f;
    constexpr float ROUND_TIME = 99.0f;  // seconds
}

// State of a single projectile
struct ProjectileState {
    glm::vec3 position{0.0f};
    glm::vec3 velocity{0.0f};
    uint8_t ownerID = 0;      // 0 or 1 (which player shot it)
    float damage = GameConstants::PROJECTILE_DAMAGE;
    bool active = true;       // false = should be removed

    void Serialize(char* buffer, size_t& offset) const {
        memcpy(buffer + offset, &position, sizeof(position));
        offset += sizeof(position);
        memcpy(buffer + offset, &velocity, sizeof(velocity));
        offset += sizeof(velocity);
        memcpy(buffer + offset, &ownerID, sizeof(ownerID));
        offset += sizeof(ownerID);
        memcpy(buffer + offset, &damage, sizeof(damage));
        offset += sizeof(damage);
        memcpy(buffer + offset, &active, sizeof(active));
        offset += sizeof(active);
    }

    void Deserialize(const char* buffer, size_t& offset) {
        memcpy(&position, buffer + offset, sizeof(position));
        offset += sizeof(position);
        memcpy(&velocity, buffer + offset, sizeof(velocity));
        offset += sizeof(velocity);
        memcpy(&ownerID, buffer + offset, sizeof(ownerID));
        offset += sizeof(ownerID);
        memcpy(&damage, buffer + offset, sizeof(damage));
        offset += sizeof(damage);
        memcpy(&active, buffer + offset, sizeof(active));
        offset += sizeof(active);
    }

    static constexpr size_t SerializedSize() {
        return sizeof(glm::vec3) * 2 + sizeof(uint8_t) + sizeof(float) + sizeof(bool);
    }
};

// State of a single player
struct PlayerState {
    glm::vec3 position{0.0f};
    glm::vec3 velocity{0.0f};
    float facingAngle = 0.0f;  // degrees, for aiming projectiles
    float hp = GameConstants::STARTING_HP;
    float projectileCooldown = 0.0f;  // time until can shoot again
    uint8_t roundWins = 0;
    bool alive = true;

    void Serialize(char* buffer, size_t& offset) const {
        memcpy(buffer + offset, &position, sizeof(position));
        offset += sizeof(position);
        memcpy(buffer + offset, &velocity, sizeof(velocity));
        offset += sizeof(velocity);
        memcpy(buffer + offset, &facingAngle, sizeof(facingAngle));
        offset += sizeof(facingAngle);
        memcpy(buffer + offset, &hp, sizeof(hp));
        offset += sizeof(hp);
        memcpy(buffer + offset, &projectileCooldown, sizeof(projectileCooldown));
        offset += sizeof(projectileCooldown);
        memcpy(buffer + offset, &roundWins, sizeof(roundWins));
        offset += sizeof(roundWins);
        memcpy(buffer + offset, &alive, sizeof(alive));
        offset += sizeof(alive);
    }

    void Deserialize(const char* buffer, size_t& offset) {
        memcpy(&position, buffer + offset, sizeof(position));
        offset += sizeof(position);
        memcpy(&velocity, buffer + offset, sizeof(velocity));
        offset += sizeof(velocity);
        memcpy(&facingAngle, buffer + offset, sizeof(facingAngle));
        offset += sizeof(facingAngle);
        memcpy(&hp, buffer + offset, sizeof(hp));
        offset += sizeof(hp);
        memcpy(&projectileCooldown, buffer + offset, sizeof(projectileCooldown));
        offset += sizeof(projectileCooldown);
        memcpy(&roundWins, buffer + offset, sizeof(roundWins));
        offset += sizeof(roundWins);
        memcpy(&alive, buffer + offset, sizeof(alive));
        offset += sizeof(alive);
    }

    static constexpr size_t SerializedSize() {
        return sizeof(glm::vec3) * 2 +  // position, velocity
               sizeof(float) * 3 +       // facingAngle, hp, cooldown
               sizeof(uint8_t) +         // roundWins
               sizeof(bool);             // alive
    }
};

// Complete game state - everything needed to render/simulate one frame
struct GameState {
    PlayerState players[2];
    std::vector<ProjectileState> projectiles;
    uint32_t frameNumber = 0;
    float roundTimer = GameConstants::ROUND_TIME;
    uint8_t currentRound = 1;  // 1, 2, or 3

    // Reset for new round (keep round wins)
    void ResetRound() {
        for (int i = 0; i < 2; i++) {
            players[i].position = (i == 0) ? glm::vec3(-5.0f, 0.0f, 0.0f) : glm::vec3(5.0f, 0.0f, 0.0f);
            players[i].velocity = glm::vec3(0.0f);
            players[i].facingAngle = (i == 0) ? 0.0f : 180.0f;
            players[i].hp = GameConstants::STARTING_HP;
            players[i].projectileCooldown = 0.0f;
            players[i].alive = true;
        }
        projectiles.clear();
        roundTimer = GameConstants::ROUND_TIME;
    }

    // Reset for new match
    void ResetMatch() {
        for (int i = 0; i < 2; i++) {
            players[i].roundWins = 0;
        }
        currentRound = 1;
        ResetRound();
        frameNumber = 0;
    }

    // Serialize entire game state to buffer
    void Serialize(char* buffer, size_t& outSize) const {
        size_t offset = 0;

        // Players
        for (int i = 0; i < 2; i++) {
            players[i].Serialize(buffer, offset);
        }

        // Projectile count
        uint16_t projCount = static_cast<uint16_t>(projectiles.size());
        memcpy(buffer + offset, &projCount, sizeof(projCount));
        offset += sizeof(projCount);

        // Projectiles
        for (const auto& proj : projectiles) {
            proj.Serialize(buffer, offset);
        }

        // Frame number and round info
        memcpy(buffer + offset, &frameNumber, sizeof(frameNumber));
        offset += sizeof(frameNumber);
        memcpy(buffer + offset, &roundTimer, sizeof(roundTimer));
        offset += sizeof(roundTimer);
        memcpy(buffer + offset, &currentRound, sizeof(currentRound));
        offset += sizeof(currentRound);

        outSize = offset;
    }

    // Deserialize from buffer
    void Deserialize(const char* buffer, size_t size) {
        size_t offset = 0;

        // Players
        for (int i = 0; i < 2; i++) {
            players[i].Deserialize(buffer, offset);
        }

        // Projectile count
        uint16_t projCount = 0;
        memcpy(&projCount, buffer + offset, sizeof(projCount));
        offset += sizeof(projCount);

        // Projectiles
        projectiles.clear();
        projectiles.reserve(projCount);
        for (uint16_t i = 0; i < projCount; i++) {
            ProjectileState proj;
            proj.Deserialize(buffer, offset);
            projectiles.push_back(proj);
        }

        // Frame number and round info
        memcpy(&frameNumber, buffer + offset, sizeof(frameNumber));
        offset += sizeof(frameNumber);
        memcpy(&roundTimer, buffer + offset, sizeof(roundTimer));
        offset += sizeof(roundTimer);
        memcpy(&currentRound, buffer + offset, sizeof(currentRound));
        offset += sizeof(currentRound);
    }

    // Estimate max serialized size (for buffer allocation)
    size_t MaxSerializedSize() const {
        return PlayerState::SerializedSize() * 2 +
               sizeof(uint16_t) +  // projectile count
               ProjectileState::SerializedSize() * projectiles.size() +
               sizeof(frameNumber) + sizeof(roundTimer) + sizeof(currentRound);
    }
};

#endif
