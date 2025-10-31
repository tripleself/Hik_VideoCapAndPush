#include "ControlServer.h"
#include <iostream>
#include <algorithm>

#pragma comment(lib, "ws2_32.lib")

static bool ensureWinsockInitialized()
{
    static std::atomic<bool> inited{false};
    static std::mutex initMutex;
    if (inited.load())
        return true;
    std::lock_guard<std::mutex> lock(initMutex);
    if (inited.load())
        return true;
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        std::cerr << "[ControlServer] Winsock initialization failed" << std::endl;
        return false;
    }
    inited.store(true);
    return true;
}

void ControlServer::closesocketSafe(SOCKET &s)
{
    if (s != INVALID_SOCKET)
    {
        closesocket(s);
        s = INVALID_SOCKET;
    }
}

ControlServer::ControlServer()
    : running_(false), port_(0), listenSocket_(INVALID_SOCKET)
{
}

ControlServer::~ControlServer()
{
    stop();
}

bool ControlServer::start(uint16_t port)
{
    if (!ensureWinsockInitialized())
        return false;

    stop();
    port_ = port;
    running_ = true;

    listenSocket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSocket_ == INVALID_SOCKET)
    {
        std::cerr << "[ControlServer] Failed to create listen socket, err=" << WSAGetLastError() << std::endl;
        running_ = false;
        return false;
    }

    // allow reuse
    int reuse = 1;
    setsockopt(listenSocket_, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse, sizeof(reuse));

    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port_);
    if (bind(listenSocket_, (sockaddr *)&addr, sizeof(addr)) == SOCKET_ERROR)
    {
        std::cerr << "[ControlServer] Bind failed, err=" << WSAGetLastError() << std::endl;
        closesocketSafe(listenSocket_);
        running_ = false;
        return false;
    }
    if (listen(listenSocket_, SOMAXCONN) == SOCKET_ERROR)
    {
        std::cerr << "[ControlServer] Listen failed, err=" << WSAGetLastError() << std::endl;
        closesocketSafe(listenSocket_);
        running_ = false;
        return false;
    }

    acceptThread_ = std::thread(&ControlServer::acceptLoop, this);
    std::cout << "[ControlServer] Listening on port " << port_ << std::endl;
    return true;
}

void ControlServer::stop()
{
    if (!running_)
        return;
    running_ = false;

    // close listen socket to unblock accept
    closesocketSafe(listenSocket_);

    if (acceptThread_.joinable())
        acceptThread_.join();

    // close all clients
    {
        std::lock_guard<std::mutex> lock(clientsMutex_);
        for (auto &s : clients_)
            closesocketSafe(s);
        clients_.clear();
    }
}

void ControlServer::acceptLoop()
{
    while (running_)
    {
        sockaddr_in caddr;
        int clen = sizeof(caddr);
        SOCKET cs = accept(listenSocket_, (sockaddr *)&caddr, &clen);
        if (cs == INVALID_SOCKET)
        {
            if (running_)
            {
                std::cerr << "[ControlServer] accept error, err=" << WSAGetLastError() << std::endl;
            }
            continue;
        }
        {
            std::lock_guard<std::mutex> lock(clientsMutex_);
            clients_.push_back(cs);
        }
        std::thread(&ControlServer::clientLoop, this, cs).detach();
    }
}

bool ControlServer::parseSetDirCmd(const std::string &line, int &outDir)
{
    // Expect exactly: CMD:SET_DIR:1 or CMD:SET_DIR:2 (with or without trailing CR)
    std::string s = line;
    if (!s.empty() && (s.back() == '\r' || s.back() == '\n'))
    {
        while (!s.empty() && (s.back() == '\r' || s.back() == '\n'))
            s.pop_back();
    }
    const std::string prefix = "CMD:SET_DIR:";
    if (s.rfind(prefix, 0) != 0)
        return false;
    if (s.size() <= prefix.size())
        return false;
    char c = s[prefix.size()];
    if (c == '1')
    {
        outDir = 1;
        return true;
    }
    if (c == '2')
    {
        outDir = 2;
        return true;
    }
    return false;
}

void ControlServer::broadcastLine(const std::string &line)
{
    std::lock_guard<std::mutex> lock(clientsMutex_);
    // 移除空客户端列表检查，确保单个客户端也能收到自己发送的CMD对应的NOTIFY
    // if (clients_.empty())
    //     return;

    std::vector<SOCKET> toRemove;
    for (auto s : clients_)
    {
        int r = send(s, line.c_str(), (int)line.size(), 0);
        if (r == SOCKET_ERROR)
        {
            toRemove.push_back(s);
        }
        else
        {
            std::cout << "[ControlServer] Send CMD successfully" << std::endl;
        }
    }
    if (!toRemove.empty())
    {
        for (auto s : toRemove)
        {
            auto it = std::find(clients_.begin(), clients_.end(), s);
            if (it != clients_.end())
            {
                closesocketSafe(*it);
                clients_.erase(it);
            }
        }
    }
}

void ControlServer::clientLoop(SOCKET clientSocket)
{
    const int BUF = 512;
    char buffer[BUF];
    std::string acc;

    while (running_)
    {
        int n = recv(clientSocket, buffer, BUF, 0);
        if (n <= 0)
            break;
        acc.append(buffer, buffer + n);
        // process lines
        size_t pos;
        while ((pos = acc.find('\n')) != std::string::npos)
        {
            std::string line = acc.substr(0, pos + 1);
            acc.erase(0, pos + 1);

            int dir = 0;
            if (parseSetDirCmd(line, dir))
            {
                std::string notify = std::string("NOTIFY:SHOW_DIR:") + (dir == 1 ? "1\n" : "2\n");
                broadcastLine(notify);
            }
            // ignore unknown lines silently
        }
    }

    // remove client
    {
        std::lock_guard<std::mutex> lock(clientsMutex_);
        auto it = std::find(clients_.begin(), clients_.end(), clientSocket);
        if (it != clients_.end())
        {
            closesocketSafe(*it);
            clients_.erase(it);
        }
    }
}
