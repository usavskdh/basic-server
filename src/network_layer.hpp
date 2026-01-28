#ifndef NETWORK_LAYER_H
#define NETWORK_LAYER_H

// Include ENet before GLFW to avoid APIENTRY redefinition warning
#include <enet/enet.h>

#include "input_state.hpp"
#include "game_state.hpp"

#include <functional>
#include <string>
#include <queue>
#include <cstdint>

// Connection state
enum class ConnectionState {
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    FAILED
};

// Packet types for our protocol
enum class NetPacketType : uint8_t {
    INPUT = 1,          // Client → Server: player input
    GAME_STATE = 2,     // Server → Client: full game state
    PLAYER_JOINED = 3,  // Server → Client: player ID assignment
    GAME_START = 4,     // Server → Clients: match is starting
    ROUND_END = 5,      // Server → Clients: round ended
    MATCH_END = 6,      // Server → Clients: match ended
};

// Abstract network interface
// Can swap implementations (client-server vs rollback) without changing game code
class INetworkLayer {
public:
    virtual ~INetworkLayer() = default;

    // Connection
    virtual bool Connect(const std::string& host, uint16_t port) = 0;
    virtual void Disconnect() = 0;
    virtual ConnectionState GetState() const = 0;

    // Send/receive
    virtual void SendInput(const InputState& input) = 0;
    virtual void SendGameState(const GameState& state) = 0;
    virtual void Update() = 0;  // Process network events

    // Callbacks (set by game code)
    std::function<void(const InputState&, int playerIndex)> OnInputReceived;
    std::function<void(const GameState&)> OnGameStateReceived;
    std::function<void(int playerIndex)> OnPlayerJoined;
    std::function<void()> OnGameStart;
    std::function<void(int winner)> OnRoundEnd;
    std::function<void(int winner)> OnMatchEnd;
    std::function<void(int playerIndex)> OnDisconnected;
};

// =============================================================================
// Client-Server Implementation using ENet
// =============================================================================

class ClientNetwork : public INetworkLayer {
public:
    ClientNetwork() {
        if (enet_initialize() != 0) {
            // Handle error
        }
    }

    ~ClientNetwork() override {
        Disconnect();
        enet_deinitialize();
    }

    bool Connect(const std::string& host, uint16_t port) override {
        client = enet_host_create(nullptr, 1, 2, 0, 0);
        if (!client) return false;

        ENetAddress address;
        enet_address_set_host(&address, host.c_str());
        address.port = port;

        peer = enet_host_connect(client, &address, 2, 0);
        if (!peer) {
            enet_host_destroy(client);
            client = nullptr;
            return false;
        }

        state = ConnectionState::CONNECTING;
        return true;
    }

    void Disconnect() override {
        if (peer) {
            enet_peer_disconnect(peer, 0);
            peer = nullptr;
        }
        if (client) {
            enet_host_destroy(client);
            client = nullptr;
        }
        state = ConnectionState::DISCONNECTED;
    }

    ConnectionState GetState() const override {
        return state;
    }

    void SendInput(const InputState& input) override {
        if (state != ConnectionState::CONNECTED || !peer) return;

        char buffer[64];
        size_t size = 0;

        buffer[0] = static_cast<char>(NetPacketType::INPUT);
        size = 1;

        size_t inputSize;
        input.Serialize(buffer + size, inputSize);
        size += inputSize;

        ENetPacket* packet = enet_packet_create(buffer, size, ENET_PACKET_FLAG_RELIABLE);
        enet_peer_send(peer, 0, packet);
    }

    void SendGameState(const GameState& state) override {
        // Clients don't send game state in client-server model
    }

    void Update() override {
        if (!client) return;

        ENetEvent event;
        while (enet_host_service(client, &event, 0) > 0) {
            switch (event.type) {
                case ENET_EVENT_TYPE_CONNECT:
                    state = ConnectionState::CONNECTED;
                    break;

                case ENET_EVENT_TYPE_RECEIVE:
                    ProcessPacket(event.packet->data, event.packet->dataLength);
                    enet_packet_destroy(event.packet);
                    break;

                case ENET_EVENT_TYPE_DISCONNECT:
                    state = ConnectionState::DISCONNECTED;
                    peer = nullptr;
                    if (OnDisconnected) OnDisconnected(-1);
                    break;

                default:
                    break;
            }
        }
    }

    int GetLocalPlayerIndex() const { return localPlayerIndex; }

private:
    void ProcessPacket(const uint8_t* data, size_t length) {
        if (length < 1) return;

        NetPacketType type = static_cast<NetPacketType>(data[0]);

        switch (type) {
            case NetPacketType::GAME_STATE: {
                GameState gameState;
                gameState.Deserialize(reinterpret_cast<const char*>(data + 1), length - 1);
                if (OnGameStateReceived) OnGameStateReceived(gameState);
                break;
            }

            case NetPacketType::PLAYER_JOINED: {
                if (length >= 2) {
                    localPlayerIndex = data[1];
                    if (OnPlayerJoined) OnPlayerJoined(localPlayerIndex);
                }
                break;
            }

            case NetPacketType::GAME_START: {
                if (OnGameStart) OnGameStart();
                break;
            }

            case NetPacketType::ROUND_END: {
                if (length >= 2 && OnRoundEnd) OnRoundEnd(data[1]);
                break;
            }

            case NetPacketType::MATCH_END: {
                if (length >= 2 && OnMatchEnd) OnMatchEnd(data[1]);
                break;
            }

            default:
                break;
        }
    }

    ENetHost* client = nullptr;
    ENetPeer* peer = nullptr;
    ConnectionState state = ConnectionState::DISCONNECTED;
    int localPlayerIndex = 0;
};

// =============================================================================
// Server Implementation using ENet
// =============================================================================

class ServerNetwork : public INetworkLayer {
public:
    ServerNetwork() {
        if (enet_initialize() != 0) {
            // Handle error
        }
    }

    ~ServerNetwork() override {
        Disconnect();
        enet_deinitialize();
    }

    bool Connect(const std::string& host, uint16_t port) override {
        // For server, "Connect" means start listening
        ENetAddress address;
        address.host = ENET_HOST_ANY;
        address.port = port;

        server = enet_host_create(&address, 2, 2, 0, 0);  // Max 2 clients for 1v1
        if (!server) return false;

        state = ConnectionState::CONNECTED;
        return true;
    }

    void Disconnect() override {
        // Disconnect all peers
        for (int i = 0; i < 2; i++) {
            if (peers[i]) {
                enet_peer_disconnect(peers[i], 0);
                peers[i] = nullptr;
            }
        }
        if (server) {
            enet_host_destroy(server);
            server = nullptr;
        }
        state = ConnectionState::DISCONNECTED;
    }

    ConnectionState GetState() const override {
        return state;
    }

    void SendInput(const InputState& input) override {
        // Server doesn't send inputs
    }

    void SendGameState(const GameState& gameState) override {
        if (state != ConnectionState::CONNECTED) return;

        // Allocate buffer for game state
        size_t maxSize = gameState.MaxSerializedSize() + 1;
        char* buffer = new char[maxSize];

        buffer[0] = static_cast<char>(NetPacketType::GAME_STATE);
        size_t size = 1;

        size_t stateSize;
        gameState.Serialize(buffer + size, stateSize);
        size += stateSize;

        // Send to all connected clients
        for (int i = 0; i < 2; i++) {
            if (peers[i]) {
                ENetPacket* packet = enet_packet_create(buffer, size, ENET_PACKET_FLAG_RELIABLE);
                enet_peer_send(peers[i], 0, packet);
            }
        }

        delete[] buffer;
    }

    void Update() override {
        if (!server) return;

        ENetEvent event;
        while (enet_host_service(server, &event, 0) > 0) {
            switch (event.type) {
                case ENET_EVENT_TYPE_CONNECT: {
                    // Check for stale/disconnected peers and clean them up first
                    for (int i = 0; i < 2; i++) {
                        if (peers[i] && peers[i]->state == ENET_PEER_STATE_DISCONNECTED) {
                            peers[i] = nullptr;
                            while (!pendingInputs[i].empty()) {
                                pendingInputs[i].pop();
                            }
                        }
                    }

                    // Find empty slot (prefer slot 0)
                    int slot = -1;
                    for (int i = 0; i < 2; i++) {
                        if (!peers[i]) {
                            slot = i;
                            peers[i] = event.peer;
                            break;
                        }
                    }

                    if (slot >= 0) {
                        // Send player their index
                        char data[2] = { static_cast<char>(NetPacketType::PLAYER_JOINED), static_cast<char>(slot) };
                        ENetPacket* packet = enet_packet_create(data, 2, ENET_PACKET_FLAG_RELIABLE);
                        enet_peer_send(event.peer, 0, packet);

                        if (OnPlayerJoined) OnPlayerJoined(slot);

                        // Check if both players connected
                        if (peers[0] && peers[1]) {
                            // Start game
                            char startData[1] = { static_cast<char>(NetPacketType::GAME_START) };
                            for (int i = 0; i < 2; i++) {
                                ENetPacket* startPacket = enet_packet_create(startData, 1, ENET_PACKET_FLAG_RELIABLE);
                                enet_peer_send(peers[i], 0, startPacket);
                            }
                            if (OnGameStart) OnGameStart();
                        }
                    } else {
                        // Server full, disconnect
                        enet_peer_disconnect(event.peer, 0);
                    }
                    break;
                }

                case ENET_EVENT_TYPE_RECEIVE:
                    ProcessPacket(event.peer, event.packet->data, event.packet->dataLength);
                    enet_packet_destroy(event.packet);
                    break;

                case ENET_EVENT_TYPE_DISCONNECT: {
                    // Find which player disconnected
                    for (int i = 0; i < 2; i++) {
                        if (peers[i] == event.peer) {
                            peers[i] = nullptr;
                            if (OnDisconnected) OnDisconnected(i);
                            break;
                        }
                    }
                    break;
                }

                default:
                    break;
            }
        }
    }

    // Get pending inputs for a player
    bool GetPendingInput(int playerIndex, InputState& outInput) {
        if (playerIndex < 0 || playerIndex > 1) return false;
        if (pendingInputs[playerIndex].empty()) return false;

        outInput = pendingInputs[playerIndex].front();
        pendingInputs[playerIndex].pop();
        return true;
    }

    bool HasBothPlayers() const {
        return peers[0] != nullptr && peers[1] != nullptr;
    }

private:
    void ProcessPacket(ENetPeer* peer, const uint8_t* data, size_t length) {
        if (length < 1) return;

        // Find player index
        int playerIndex = -1;
        for (int i = 0; i < 2; i++) {
            if (peers[i] == peer) {
                playerIndex = i;
                break;
            }
        }
        if (playerIndex < 0) return;

        NetPacketType type = static_cast<NetPacketType>(data[0]);

        switch (type) {
            case NetPacketType::INPUT: {
                InputState input;
                input.Deserialize(reinterpret_cast<const char*>(data + 1), length - 1);
                pendingInputs[playerIndex].push(input);
                if (OnInputReceived) OnInputReceived(input, playerIndex);
                break;
            }

            default:
                break;
        }
    }

    ENetHost* server = nullptr;
    ENetPeer* peers[2] = { nullptr, nullptr };
    ConnectionState state = ConnectionState::DISCONNECTED;
    std::queue<InputState> pendingInputs[2];
};

#endif
