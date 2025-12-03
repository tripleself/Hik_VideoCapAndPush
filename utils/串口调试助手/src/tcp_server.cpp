#include "ProtocolParser.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <fstream>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#define SOCKET int
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define closesocket close
#endif

class TCPServer
{
private:
    SOCKET serverSocket;
    std::vector<SOCKET> clients;
    ProtocolParser parser;
    bool running;
    std::vector<std::vector<uint8_t>> dataFrames;
    size_t currentFrameIndex;

    // 从保存的数据文件中读取真实数据
    std::vector<uint8_t> loadRealSerialData()
    {
        std::vector<uint8_t> data;

        // 使用SaveWindows文件中的第一行数据作为示例
        // [16:09:45.279]发送：10 02 00 50 11 00 01 00 05 00 38 00 67 01 00 01 20 20 20 20 00 00 00 00 00 08 00 00 00 00 00 00 00 00 00 00 00 08 84 D7 00 74 39 C0 05 47 02 27 66 00 00 00 06 02 FF FF 02 3C 09 00 20 03 2D 00 03 08 84 D7 00 91 9F 12 25 15 01 00 01 00 00 01 00 D8 DF 16 10 03

        uint8_t realData[] = {
            0x10, 0x02, 0x00, 0x50, 0x11, 0x00, 0x01, 0x00, 0x05, 0x00,
            0x38, 0x00, 0x67, 0x01, 0x00, 0x01, 0x20, 0x20, 0x20, 0x20,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x84, 0xD7,
            0x00, 0x74, 0x39, 0xC0, 0x05, 0x47, 0x02, 0x27, 0x66, 0x00,
            0x00, 0x00, 0x06, 0x02, 0xFF, 0xFF, 0x02, 0x3C, 0x09, 0x00,
            0x20, 0x03, 0x2D, 0x00, 0x03, 0x08, 0x84, 0xD7, 0x00, 0x91,
            0x9F, 0x12, 0x25, 0x15, 0x01, 0x00, 0x01, 0x00, 0x00, 0x01,
            0x00, 0xD8, 0xDF, 0x16, 0x10, 0x03};

        size_t dataSize = sizeof(realData);
        data.assign(realData, realData + dataSize);

        std::cout << "加载真实数据，长度: " << dataSize << " 字节" << std::endl;

        return data;
    }

    // 解析十六进制字符串为字节数组（用于从文件读取数据）
    std::vector<uint8_t> parseHexString(const std::string &hexStr)
    {
        std::vector<uint8_t> data;
        for (size_t i = 0; i < hexStr.length(); i += 3)
        { // 每3个字符一个字节（包括空格）
            if (i + 1 < hexStr.length())
            {
                std::string byteStr = hexStr.substr(i, 2);
                try
                {
                    uint8_t byte = static_cast<uint8_t>(std::stoul(byteStr, nullptr, 16));
                    data.push_back(byte);
                }
                catch (...)
                {
                    // 跳过无效字符
                }
            }
        }
        return data;
    }

    // 加载所有数据帧
    void loadAllDataFrames()
    {
        dataFrames.clear();

        // 添加SaveWindows文件中的真实数据帧，包含时间解析.md中提到的"5A 02 27 66"模式
        std::vector<std::string> hexDataLines = {
            // 第一帧：包含时间数据 5A 02 27 66
            "10 02 00 50 11 00 01 00 05 00 38 00 67 01 00 01 20 20 20 20 00 00 00 00 00 08 00 00 00 00 00 00 00 00 00 00 00 08 84 D7 00 74 39 C0 05 47 02 27 66 00 00 00 06 02 FF FF 02 3C 09 00 20 03 2D 00 03 08 84 D7 00 91 9F 12 25 15 01 00 01 00 00 01 00 D8 DF 16 10 03",
            // 第二帧：稍后的时间数据
            "10 02 00 50 11 00 01 00 05 00 38 00 67 01 00 01 20 20 20 20 00 00 00 00 00 08 00 00 00 00 00 00 00 00 00 00 00 08 84 D7 00 74 39 C0 05 48 02 27 66 00 00 00 06 02 FF FF 02 3C 09 00 20 03 2D 00 03 08 84 D7 00 91 9F 12 25 15 01 00 01 00 00 01 00 81 2B C6 10 03",
            // 第三帧：更晚的时间数据
            "10 02 00 50 11 00 01 00 05 00 38 00 67 01 00 01 20 20 20 20 00 00 00 00 00 08 00 00 00 00 00 00 00 00 00 00 00 08 84 D7 00 74 39 C0 05 49 02 27 66 00 00 00 06 02 FF FF 02 3C 09 00 20 03 2D 00 03 08 84 D7 00 91 9F 12 25 15 01 00 01 00 00 01 00 29 EB 6F 10 03"};

        for (const auto &hexLine : hexDataLines)
        {
            std::vector<uint8_t> frame = parseHexString(hexLine);
            if (!frame.empty())
            {
                dataFrames.push_back(frame);
            }
        }

        currentFrameIndex = 0;
        std::cout << "加载了 " << dataFrames.size() << " 个数据帧" << std::endl;
    }

    // 获取下一个数据帧
    std::vector<uint8_t> getNextDataFrame()
    {
        if (dataFrames.empty())
        {
            return loadRealSerialData(); // 回退到原始方法
        }

        std::vector<uint8_t> frame = dataFrames[currentFrameIndex];
        currentFrameIndex = (currentFrameIndex + 1) % dataFrames.size(); // 循环使用数据帧

        return frame;
    }

public:
    TCPServer() : serverSocket(INVALID_SOCKET), running(false), currentFrameIndex(0)
    {
#ifdef _WIN32
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
        {
            std::cerr << "WSAStartup失败" << std::endl;
            exit(1);
        }
#endif
    }

    ~TCPServer()
    {
        stop();
#ifdef _WIN32
        WSACleanup();
#endif
    }

    bool start(int port = 8888)
    {
        // 创建socket
        serverSocket = socket(AF_INET, SOCK_STREAM, 0);
        if (serverSocket == INVALID_SOCKET)
        {
            std::cerr << "创建socket失败" << std::endl;
            return false;
        }

        // 设置socket选项
        int opt = 1;
#ifdef _WIN32
        setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt));
#else
        setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

        // 绑定地址
        struct sockaddr_in serverAddr;
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_addr.s_addr = INADDR_ANY;
        serverAddr.sin_port = htons(port);

        if (bind(serverSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
        {
            std::cerr << "绑定端口失败" << std::endl;
            return false;
        }

        // 开始监听
        if (listen(serverSocket, 5) == SOCKET_ERROR)
        {
            std::cerr << "监听失败" << std::endl;
            return false;
        }

        running = true;
        std::cout << "TCP服务器启动成功，监听端口: " << port << std::endl;

        // 加载数据帧
        loadAllDataFrames();

        // 启动接受连接的线程
        std::thread acceptThread(&TCPServer::acceptClients, this);
        acceptThread.detach();

        // 启动数据发送线程
        std::thread dataThread(&TCPServer::sendData, this);
        dataThread.detach();

        return true;
    }

    void stop()
    {
        running = false;

        // 关闭所有客户端连接
        for (SOCKET client : clients)
        {
            closesocket(client);
        }
        clients.clear();

        // 关闭服务器socket
        if (serverSocket != INVALID_SOCKET)
        {
            closesocket(serverSocket);
            serverSocket = INVALID_SOCKET;
        }
    }

    void acceptClients()
    {
        while (running)
        {
            struct sockaddr_in clientAddr;
            int clientAddrLen = sizeof(clientAddr);

            SOCKET clientSocket = accept(serverSocket, (struct sockaddr *)&clientAddr, &clientAddrLen);
            if (clientSocket != INVALID_SOCKET)
            {
                clients.push_back(clientSocket);
                std::cout << "新客户端连接: " << inet_ntoa(clientAddr.sin_addr) << std::endl;
            }
        }
    }

    void sendData()
    {
        while (running)
        {
            if (!clients.empty())
            {
                // 获取下一个数据帧
                std::vector<uint8_t> rawData = getNextDataFrame();

                // 解析数据
                ProtocolParser::ParsedData parsedData = parser.parseFrame(rawData.data(), rawData.size());

                if (parsedData.isValid)
                {
                    std::string jsonData = parser.toJsonString(parsedData);
                    std::cout << "发送数据: " << jsonData << std::endl;

                    // 发送给所有客户端
                    auto it = clients.begin();
                    while (it != clients.end())
                    {
                        int result = send(*it, jsonData.c_str(), jsonData.length(), 0);
                        if (result == SOCKET_ERROR)
                        {
                            std::cout << "客户端断开连接" << std::endl;
                            closesocket(*it);
                            it = clients.erase(it);
                        }
                        else
                        {
                            ++it;
                        }
                    }
                }
            }

            // 每2秒发送一次数据
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }
    }

    void run()
    {
        std::cout << "服务器运行中... 按Enter键停止" << std::endl;
        std::cin.get();
        stop();
    }
};

int main()
{
    // 设置控制台编码为UTF-8（Windows）
#ifdef _WIN32
    system("chcp 65001");
#endif

    std::cout << "=== GYK协议数据解析TCP服务器 ===" << std::endl;

    TCPServer server;
    if (server.start(8888))
    {
        server.run();
    }

    std::cout << "服务器已停止" << std::endl;
    return 0;
}
