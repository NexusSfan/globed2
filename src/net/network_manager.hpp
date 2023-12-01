#pragma once
#include "game_socket.hpp"
#include <defs.hpp>

#include <managers/server_manager.hpp>
#include <util/sync.hpp>
#include <util/time.hpp>

#include <functional>
#include <unordered_map>
#include <thread>

using namespace util::sync;

enum class NetworkThreadTask {
    PingServers
};

template <typename T>
concept HasPacketID = requires { T::PACKET_ID; };

struct PollBothResult {
    bool hasNormal;
    bool hasPing;
};

// This class is fully thread safe..? hell do i know..
class NetworkManager {
public:
    using PacketCallback = std::function<void(std::shared_ptr<Packet>)>;

    template <HasPacketID Pty>
    using PacketCallbackSpecific = std::function<void(Pty*)>;

    static constexpr uint8_t PROTOCOL_VERSION = 1;

    GLOBED_SINGLETON(NetworkManager)
    NetworkManager();
    ~NetworkManager();

    // Connect to a server
    void connect(const std::string& addr, unsigned short port);
    // Safer version of `connect`, sets the active game server in `GlobedServerManager` doesn't throw an exception on error
    void connectWithView(const GameServerView& gsview);

    // Disconnect from a server. Does nothing if not connected
    void disconnect(bool quiet = false);

    // Sends a packet to the currently established connection. Throws if disconnected.
    void send(std::shared_ptr<Packet> packet);

    // Adds a packet listener and calls your callback function when a packet with `id` is received.
    // If there already was a callback with this packet ID, it gets replaced.
    // All callbacks are ran in the main (GD) thread.
    void addListener(packetid_t id, PacketCallback callback);

    // Same as addListener(packetid_t, PacketCallback) but hacky syntax xd
    template <HasPacketID Pty>
    void addListener(PacketCallbackSpecific<Pty> callback) {
        this->addListener(Pty::PACKET_ID, [callback](std::shared_ptr<Packet> pkt) {
            callback(static_cast<Pty*>(pkt.get()));
        });
    }

    // Removes a listener by packet ID.
    void removeListener(packetid_t id);

    // Same as removeListener(packetid_t) but hacky syntax once again
    template <HasPacketID T>
    void removeListener() {
        this->removeListener(T::PACKET_ID);
    }

    // Removes all listeners.
    void removeAllListeners();

    // queues task for pinging servers
    void taskPingServers();

    // Returns true if ANY connection has been made with a server. The handshake might not have been done at this point.
    bool connected();

    // Returns true ONLY if we are connected to a server and the crypto handshake has finished.
    bool established();

    // Returns true if we have already proved we are the account owner and are ready to rock.
    bool authenticated();

private:

    static constexpr chrono::seconds KEEPALIVE_INTERVAL = chrono::seconds(5);
    static constexpr chrono::seconds DISCONNECT_AFTER = chrono::seconds(15);

    GameSocket gameSocket;

    SmartMessageQueue<std::shared_ptr<Packet>> packetQueue;
    SmartMessageQueue<NetworkThreadTask> taskQueue;

    WrappingMutex<std::unordered_map<packetid_t, PacketCallback>> listeners;

    // threads

    void threadMainFunc();
    void threadRecvFunc();

    std::thread threadMain;
    std::thread threadRecv;

    // misc

    AtomicBool _running = true;
    AtomicBool _established = false;
    AtomicBool _loggedin = false;

    util::time::time_point lastKeepalive;
    util::time::time_point lastReceivedPacket;

    void handlePingResponse(std::shared_ptr<Packet> packet);
    void maybeSendKeepalive();
    void maybeDisconnectIfDead();

    // Builtin listeners have priority above the others.
    WrappingMutex<std::unordered_map<packetid_t, PacketCallback>> builtinListeners;

    void addBuiltinListener(packetid_t id, PacketCallback callback);

    template <HasPacketID Pty>
    void addBuiltinListener(PacketCallbackSpecific<Pty> callback) {
        this->addBuiltinListener(Pty::PACKET_ID, [callback](std::shared_ptr<Packet> pkt) {
            callback(static_cast<Pty*>(pkt.get()));
        });
    }
};