#ifndef INPUT_STATE_H
#define INPUT_STATE_H

#include <cstdint>
#include <cstring>
#include <cmath>

// InputState captures all player inputs for a single frame.
// This is the fundamental unit of network synchronization.
//
// Both clients exchange InputState each frame so they can
// simulate the same game deterministically.

struct InputState {
    // Left stick - movement (analog, -1.0 to 1.0)
    float moveX = 0.0f;
    float moveY = 0.0f;

    // Action buttons
    bool throwProjectile = false;

    // Frame number for synchronization
    uint32_t frameNumber = 0;

    // Serialize to buffer for network transmission
    void Serialize(char* buffer, size_t& outSize) const {
        size_t offset = 0;

        memcpy(buffer + offset, &moveX, sizeof(moveX));
        offset += sizeof(moveX);

        memcpy(buffer + offset, &moveY, sizeof(moveY));
        offset += sizeof(moveY);

        uint8_t buttons = 0;
        if (throwProjectile) buttons |= 0x01;
        memcpy(buffer + offset, &buttons, sizeof(buttons));
        offset += sizeof(buttons);

        memcpy(buffer + offset, &frameNumber, sizeof(frameNumber));
        offset += sizeof(frameNumber);

        outSize = offset;
    }

    // Deserialize from buffer
    void Deserialize(const char* buffer, size_t size) {
        size_t offset = 0;

        if (offset + sizeof(moveX) <= size) {
            memcpy(&moveX, buffer + offset, sizeof(moveX));
            offset += sizeof(moveX);
        }

        if (offset + sizeof(moveY) <= size) {
            memcpy(&moveY, buffer + offset, sizeof(moveY));
            offset += sizeof(moveY);
        }

        if (offset + sizeof(uint8_t) <= size) {
            uint8_t buttons = 0;
            memcpy(&buttons, buffer + offset, sizeof(buttons));
            offset += sizeof(buttons);
            throwProjectile = (buttons & 0x01) != 0;
        }

        if (offset + sizeof(frameNumber) <= size) {
            memcpy(&frameNumber, buffer + offset, sizeof(frameNumber));
            offset += sizeof(frameNumber);
        }
    }

    // Size in bytes when serialized
    static constexpr size_t SerializedSize() {
        return sizeof(float) * 2 +  // moveX, moveY
               sizeof(uint8_t) +     // buttons (packed)
               sizeof(uint32_t);     // frameNumber
    }

    // Compare two input states (useful for detecting changes)
    bool operator==(const InputState& other) const {
        return moveX == other.moveX &&
               moveY == other.moveY &&
               throwProjectile == other.throwProjectile &&
               frameNumber == other.frameNumber;
    }

    bool operator!=(const InputState& other) const {
        return !(*this == other);
    }
};

#endif // INPUT_STATE_H


// =============================================================================
// CLIENT-ONLY: GLFW Input Helpers
// Only include this part on clients that have GLFW
// =============================================================================
#ifdef GLFW_INCLUDE_NONE
// GLFW already included elsewhere
#elif defined(__has_include)
#if __has_include(<GLFW/glfw3.h>)
#define INPUT_STATE_HAS_GLFW 1
#endif
#endif

#if defined(INPUT_STATE_HAS_GLFW) || defined(INPUT_STATE_ENABLE_GLFW)
#ifndef INPUT_STATE_GLFW_HELPERS
#define INPUT_STATE_GLFW_HELPERS

#include <GLFW/glfw3.h>

// Bridge function: Read from GLFW gamepad and populate InputState
// Call this each frame to capture current inputs
inline InputState GetInputStateFromGamepad(int gamepadId, uint32_t frameNumber, float deadzone = 0.05f) {
    InputState input;
    input.frameNumber = frameNumber;

    GLFWgamepadstate state;
    if (glfwGetGamepadState(gamepadId, &state)) {
        // Left stick - movement
        float lx = state.axes[GLFW_GAMEPAD_AXIS_LEFT_X];
        float ly = state.axes[GLFW_GAMEPAD_AXIS_LEFT_Y];

        // Apply deadzone
        if (std::abs(lx) < deadzone) lx = 0.0f;
        if (std::abs(ly) < deadzone) ly = 0.0f;

        input.moveX = lx;
        input.moveY = ly;

        // A button for projectile
        input.throwProjectile = state.buttons[GLFW_GAMEPAD_BUTTON_A] == GLFW_PRESS;
    }

    return input;
}

// Alternative: Read from keyboard (for testing without gamepad)
inline InputState GetInputStateFromKeyboard(GLFWwindow* window, uint32_t frameNumber) {
    InputState input;
    input.frameNumber = frameNumber;

    // WASD movement
    float moveX = 0.0f;
    float moveY = 0.0f;

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) moveY -= 1.0f;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) moveY += 1.0f;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) moveX -= 1.0f;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) moveX += 1.0f;

    // Normalize diagonal movement
    if (moveX != 0.0f && moveY != 0.0f) {
        float len = std::sqrt(moveX * moveX + moveY * moveY);
        moveX /= len;
        moveY /= len;
    }

    input.moveX = moveX;
    input.moveY = moveY;

    // Space for projectile
    input.throwProjectile = glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS;

    return input;
}

#endif // INPUT_STATE_GLFW_HELPERS
#endif // GLFW available
