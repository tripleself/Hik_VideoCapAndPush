#pragma once

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>

/**
 * @brief A lightweight line-oriented TCP control server for handling
 *        CMD:SET_DIR:<1|2> and broadcasting NOTIFY:SHOW_DIR:<1|2> to all clients.
 *
 * Simplicity and stability first:
 * - Blocking I/O per-connection thread
 * - Minimal parsing: lines terminated by '\n'
 * - Thread-safe broadcast over a protected client socket list
 */
class ControlServer
{
public:
    ControlServer();
    ~ControlServer();

    // Start listening on the given port (e.g., 12347). Returns true on success.
    bool start(uint16_t port);

    // Stop server, close all sockets/threads gracefully.
    void stop();

private:
    // Accept loop running in a background thread
    void acceptLoop();

    // Per-client loop: read lines and handle commands
    void clientLoop(SOCKET clientSocket);

    // Broadcast a single line (must end with '\n') to all connected clients
    void broadcastLine(const std::string &line);

    // Parse a line like "CMD:SET_DIR:1"; returns true and sets outDir to 1 or 2 on success
    static bool parseSetDirCmd(const std::string &line, int &outDir);

private:
    // Server state
    std::atomic<bool> running_;
    uint16_t port_;
    SOCKET listenSocket_;
    std::thread acceptThread_;

    // Connected clients
    std::mutex clientsMutex_;
    std::vector<SOCKET> clients_;

    // Utility
    static void closesocketSafe(SOCKET &s);
};

