#include <iostream>
#include <string>
#include <thread>
#include <chrono>

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

class TCPClient
{
private:
    SOCKET clientSocket;
    bool connected;

public:
    TCPClient() : clientSocket(INVALID_SOCKET), connected(false)
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

    ~TCPClient()
    {
        disconnect();
#ifdef _WIN32
        WSACleanup();
#endif
    }

    bool connect(const std::string &serverIP = "127.0.0.1", int port = 8888)
    {
        // 创建socket
        clientSocket = socket(AF_INET, SOCK_STREAM, 0);
        if (clientSocket == INVALID_SOCKET)
        {
            std::cerr << "创建socket失败" << std::endl;
            return false;
        }

        // 设置服务器地址
        struct sockaddr_in serverAddr;
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(port);

#ifdef _WIN32
        serverAddr.sin_addr.s_addr = inet_addr(serverIP.c_str());
#else
        inet_pton(AF_INET, serverIP.c_str(), &serverAddr.sin_addr);
#endif

        // 连接到服务器
        if (::connect(clientSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
        {
            std::cerr << "连接服务器失败" << std::endl;
            closesocket(clientSocket);
            clientSocket = INVALID_SOCKET;
            return false;
        }

        connected = true;
        std::cout << "成功连接到服务器 " << serverIP << ":" << port << std::endl;
        return true;
    }

    void disconnect()
    {
        connected = false;
        if (clientSocket != INVALID_SOCKET)
        {
            closesocket(clientSocket);
            clientSocket = INVALID_SOCKET;
        }
    }

    void receiveData()
    {
        char buffer[4096];
        std::string receivedData;

        while (connected)
        {
            int bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);

            if (bytesReceived > 0)
            {
                buffer[bytesReceived] = '\0';
                receivedData += buffer;

                // 检查是否收到完整的JSON数据
                size_t startPos = receivedData.find('{');
                size_t endPos = receivedData.find('}');

                while (startPos != std::string::npos && endPos != std::string::npos && endPos > startPos)
                {
                    std::string jsonData = receivedData.substr(startPos, endPos - startPos + 1);

                    // 显示接收到的数据
                    displayParsedData(jsonData);

                    // 移除已处理的数据
                    receivedData = receivedData.substr(endPos + 1);
                    startPos = receivedData.find('{');
                    endPos = receivedData.find('}');
                }
            }
            else if (bytesReceived == 0)
            {
                std::cout << "服务器断开连接" << std::endl;
                break;
            }
            else
            {
                std::cerr << "接收数据失败" << std::endl;
                break;
            }
        }
    }

    void displayParsedData(const std::string &jsonData)
    {
        std::cout << "\n=== 接收到新数据 ===" << std::endl;
        std::cout << "时间: " << getCurrentTime() << std::endl;
        std::cout << "原始JSON: " << jsonData << std::endl;

        // 简单解析JSON并格式化显示
        std::cout << "\n--- 解析结果 ---" << std::endl;

        // 提取各个字段（简单字符串查找方式）
        std::string dateTime = extractJsonValue(jsonData, "dateTime");
        std::string actualSpeed = extractJsonValue(jsonData, "actualSpeed");
        std::string kilometerPost = extractJsonValue(jsonData, "kilometerPost");
        std::string fiveDigitTrainNumber = extractJsonValue(jsonData, "fiveDigitTrainNumber");
        std::string locomotiveNumber = extractJsonValue(jsonData, "locomotiveNumber");
        std::string isValid = extractJsonValue(jsonData, "isValid");

        std::cout << "时间: " << dateTime << std::endl;
        std::cout << "实速: " << actualSpeed << " km/h" << std::endl;
        std::cout << "公里标: " << kilometerPost << " km" << std::endl;
        std::cout << "五位车次: " << fiveDigitTrainNumber << std::endl;
        std::cout << "机车号: " << locomotiveNumber << std::endl;
        std::cout << "数据有效性: " << (isValid == "true" ? "有效" : "无效") << std::endl;
        std::cout << "========================\n"
                  << std::endl;
    }

    std::string extractJsonValue(const std::string &json, const std::string &key)
    {
        std::string searchKey = "\"" + key + "\": ";
        size_t pos = json.find(searchKey);
        if (pos == std::string::npos)
        {
            return "N/A";
        }

        pos += searchKey.length();

        // 跳过可能的引号
        if (pos < json.length() && json[pos] == '"')
        {
            pos++;
            size_t endPos = json.find('"', pos);
            if (endPos != std::string::npos)
            {
                return json.substr(pos, endPos - pos);
            }
        }
        else
        {
            // 数字或布尔值
            size_t endPos = json.find_first_of(",\n}", pos);
            if (endPos != std::string::npos)
            {
                return json.substr(pos, endPos - pos);
            }
        }

        return "N/A";
    }

    std::string getCurrentTime()
    {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);

        std::string timeStr = std::ctime(&time_t);
        // 移除换行符
        if (!timeStr.empty() && timeStr.back() == '\n')
        {
            timeStr.pop_back();
        }
        return timeStr;
    }

    void run()
    {
        std::cout << "开始接收数据... 按Ctrl+C停止" << std::endl;
        receiveData();
    }
};

int main()
{
    // 设置控制台编码为UTF-8（Windows）
#ifdef _WIN32
    system("chcp 65001");
#endif

    std::cout << "=== GYK协议数据TCP客户端 ===" << std::endl;

    TCPClient client;

    std::string serverIP;
    int port;

    std::cout << "请输入服务器IP地址 (默认127.0.0.1): ";
    std::getline(std::cin, serverIP);
    if (serverIP.empty())
    {
        serverIP = "127.0.0.1";
    }

    std::cout << "请输入端口号 (默认8888): ";
    std::string portStr;
    std::getline(std::cin, portStr);
    if (portStr.empty())
    {
        port = 8888;
    }
    else
    {
        port = std::stoi(portStr);
    }

    if (client.connect(serverIP, port))
    {
        client.run();
    }
    else
    {
        std::cout << "连接失败，请检查服务器是否启动" << std::endl;
    }

    std::cout << "客户端已退出" << std::endl;
    return 0;
}
