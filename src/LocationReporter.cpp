#include "LocationReporter.h"
#include "ObjectTrackingConfig.h"
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <chrono>
#include <ctime>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <cstdint>
#include <mutex>
#include <windows.h> // For RS422 serial communication

// 取消可能冲突的Windows宏定义
#ifdef DATA_BITS
#undef DATA_BITS
#endif
#ifdef STOP_BITS
#undef STOP_BITS
#endif
#ifdef PARITY_NONE
#undef PARITY_NONE
#endif

// GYK协议配置常量 - 内联定义避免依赖外部文件
namespace GYKProtocol
{
    // 帧标识符
    static const uint8_t FRAME_START_DLE = 0x10;
    static const uint8_t FRAME_START_STX = 0x02;
    static const uint8_t FRAME_END_DLE = 0x10;
    static const uint8_t FRAME_END_ETX = 0x03;

    // 数据字段位置（根据真实数据分析调整）
    static const int POS_FRAME_START = 0;        // 帧起始，2字节
    static const int POS_INFO_LENGTH = 2;        // 信息长度，2字节，低字节在前
    static const int POS_DATE_TIME = 45;         // 时间，4字节，实际位置46-49，后两位27 66解析为2025年
    static const int POS_ACTUAL_SPEED = 49;      // 实速，3字节，紧接着时间字段
    static const int POS_KILOMETER_POST = 57;    // 公里标，3字节
    static const int POS_FIVE_DIGIT_TRAIN = 66;  // 五位车次，2字节
    static const int POS_LOCOMOTIVE_NUMBER = 74; // 机车号，2字节

    // 数据字段长度
    static const int LEN_DATE_TIME = 4;
    static const int LEN_ACTUAL_SPEED = 3;
    static const int LEN_KILOMETER_POST = 3;
    static const int LEN_FIVE_DIGIT_TRAIN = 2;
    static const int LEN_LOCOMOTIVE_NUMBER = 2;

    // 最小帧长度
    static const int MIN_FRAME_LENGTH = 86; // 完整帧长度 (2字节起始 + 2字节长度 + 80字节数据 + 2字节结束)

    // 串口通信参数
    static const int BAUD_RATE = 9600;
    static const int DATA_BITS = 8;
    static const int STOP_BITS = 1;
    static const int PARITY_NONE = 0;
}

// 解析后的数据结构
// ParsedGYKData结构体定义已移至头文件

// 简化的RS422接口类 - 内联实现
class SimpleRS422Interface
{
private:
    HANDLE m_handle;
    bool m_isOpen;
    std::string m_lastError;

public:
    SimpleRS422Interface() : m_handle(INVALID_HANDLE_VALUE), m_isOpen(false) {}

    ~SimpleRS422Interface()
    {
        closePort();
    }

    bool openPort(const std::string &portName, int baudRate = 9600)
    {
        m_handle = CreateFileA(portName.c_str(),
                               GENERIC_READ | GENERIC_WRITE,
                               0, 0, OPEN_EXISTING,
                               FILE_ATTRIBUTE_NORMAL, 0);

        if (m_handle == INVALID_HANDLE_VALUE)
        {
            m_lastError = "无法打开串口: " + portName;
            return false;
        }

        DCB dcbSerialParams = {0};
        dcbSerialParams.DCBlength = sizeof(dcbSerialParams);

        if (!GetCommState(m_handle, &dcbSerialParams))
        {
            m_lastError = "无法获取串口状态";
            closePort();
            return false;
        }

        dcbSerialParams.BaudRate = baudRate;
        dcbSerialParams.ByteSize = GYKProtocol::DATA_BITS;
        dcbSerialParams.StopBits = ONESTOPBIT;
        dcbSerialParams.Parity = NOPARITY;

        if (!SetCommState(m_handle, &dcbSerialParams))
        {
            m_lastError = "无法设置串口参数";
            closePort();
            return false;
        }

        // 设置超时
        COMMTIMEOUTS timeouts = {0};
        timeouts.ReadIntervalTimeout = 50;
        timeouts.ReadTotalTimeoutConstant = 50;
        timeouts.ReadTotalTimeoutMultiplier = 10;
        SetCommTimeouts(m_handle, &timeouts);

        m_isOpen = true;
        return true;
    }

    void closePort()
    {
        if (m_handle != INVALID_HANDLE_VALUE)
        {
            CloseHandle(m_handle);
            m_handle = INVALID_HANDLE_VALUE;
        }
        m_isOpen = false;
    }

    int readData(uint8_t *buffer, int maxLength)
    {
        if (!m_isOpen)
            return -1;

        DWORD bytesRead;
        if (ReadFile(m_handle, buffer, maxLength, &bytesRead, NULL))
        {
            return bytesRead;
        }
        return -1;
    }

    bool isOpen() const { return m_isOpen; }
    std::string getLastError() const { return m_lastError; }
};

// 简化的协议解析器类 - 内联实现
class SimpleProtocolParser
{
public:
    ParsedGYKData parseFrame(const uint8_t *data, size_t length)
    {
        ParsedGYKData result;

        // 验证最小长度
        // std::cout << "[LocationReporter]" << length << std::endl;
        if (length < GYKProtocol::MIN_FRAME_LENGTH)
        {
            std::cout << "[LocationReporter] 帧长度不足，跳过此帧，长度为: " << length << std::endl;
            return result;
        }

        // 验证帧起始
        // std::cout << "[LocationReporter] 验证帧起始..." << std::endl;
        // std::cout << "[LocationReporter] 期望帧起始: DLE=" << std::hex << static_cast<int>(GYKProtocol::FRAME_START_DLE)
        //           << " STX=" << static_cast<int>(GYKProtocol::FRAME_START_STX) << std::dec << std::endl;
        // std::cout << "[LocationReporter] 实际帧起始: DLE=" << std::hex << static_cast<int>(data[GYKProtocol::POS_FRAME_START])
        //           << " STX=" << static_cast<int>(data[GYKProtocol::POS_FRAME_START + 1]) << std::dec << std::endl;

        if (data[GYKProtocol::POS_FRAME_START] != GYKProtocol::FRAME_START_DLE ||
            data[GYKProtocol::POS_FRAME_START + 1] != GYKProtocol::FRAME_START_STX)
        {
            std::cout << "[LocationReporter] 帧起始验证失败，跳过此帧" << std::endl;
            return result;
        }
        // std::cout << "[LocationReporter] 帧起始验证成功" << std::endl;

        try
        {
            // 解析时间
            if (length > GYKProtocol::POS_DATE_TIME + GYKProtocol::LEN_DATE_TIME - 1)
            {
                // 输出时间字段的原始十六进制数据
                // std::cout << "[LocationReporter] 时间字段原始数据 (HEX): ";
                // for (int i = 0; i < GYKProtocol::LEN_DATE_TIME; ++i)
                // {
                //     std::cout << std::hex << std::uppercase << std::setfill('0') << std::setw(2)
                //               << static_cast<int>(data[GYKProtocol::POS_DATE_TIME + i]) << " ";
                // }
                // std::cout << std::dec << std::endl;

                result.dateTime = parseBCDTime(&data[GYKProtocol::POS_DATE_TIME]);
                std::cout << "[LocationReporter] 解析后时间: " << result.dateTime << std::endl;
            }

            // 解析实速
            if (length > GYKProtocol::POS_ACTUAL_SPEED + GYKProtocol::LEN_ACTUAL_SPEED - 1)
            {
                result.actualSpeed = parseSpeed(&data[GYKProtocol::POS_ACTUAL_SPEED]);
                std::cout << "[LocationReporter] " << "实速: " << result.actualSpeed << std::endl;
            }

            // 解析公里标
            if (length > GYKProtocol::POS_KILOMETER_POST + GYKProtocol::LEN_KILOMETER_POST - 1)
            {
                result.kilometerPost = parseKilometerPost(&data[GYKProtocol::POS_KILOMETER_POST]);
                std::cout << "[LocationReporter] " << "公里标: " << result.kilometerPost << std::endl;
            }

            // 解析五位车次
            if (length > GYKProtocol::POS_FIVE_DIGIT_TRAIN + GYKProtocol::LEN_FIVE_DIGIT_TRAIN - 1)
            {
                uint16_t trainNum = data[GYKProtocol::POS_FIVE_DIGIT_TRAIN] |
                                    (data[GYKProtocol::POS_FIVE_DIGIT_TRAIN + 1] << 8);
                result.fiveDigitTrainNumber = std::to_string(trainNum);
                std::cout << "[LocationReporter] " << "五位车次: " << result.fiveDigitTrainNumber << std::endl;
            }

            // 解析机车号
            if (length > GYKProtocol::POS_LOCOMOTIVE_NUMBER + GYKProtocol::LEN_LOCOMOTIVE_NUMBER - 1)
            {
                uint16_t locoNum = data[GYKProtocol::POS_LOCOMOTIVE_NUMBER] |
                                   (data[GYKProtocol::POS_LOCOMOTIVE_NUMBER + 1] << 8);
                result.locomotiveNumber = std::to_string(locoNum);
                std::cout << "[LocationReporter] " << "机车号: " << result.locomotiveNumber << std::endl;
            }

            result.isValid = true;
        }
        catch (const std::exception &e)
        {
            result.isValid = false;
        }

        return result;
    }

private:
    std::string parseBCDTime(const uint8_t *data)
    {
        // 4字节数据，低字节在前
        uint32_t timeValue = data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);

        // std::cout << "[LocationReporter] 时间解析调试:" << std::endl;
        // std::cout << "  原始4字节: " << std::hex << std::uppercase << std::setfill('0')
        //           << std::setw(2) << static_cast<int>(data[0]) << " "
        //           << std::setw(2) << static_cast<int>(data[1]) << " "
        //           << std::setw(2) << static_cast<int>(data[2]) << " "
        //           << std::setw(2) << static_cast<int>(data[3]) << std::dec << std::endl;
        // std::cout << "  合并后的32位值: 0x" << std::hex << std::uppercase << timeValue << std::dec << std::endl;

        int second = timeValue & 0x3F;        // b5～b0：秒
        int minute = (timeValue >> 6) & 0x3F; // b11～b6：分
        int hour = (timeValue >> 12) & 0x1F;  // b16～b12：时
        int day = (timeValue >> 17) & 0x1F;   // b21～b17：日
        int month = (timeValue >> 22) & 0x0F; // b25～b22：月
        int year = (timeValue >> 26) & 0x3F;  // b31～b26：年

        // std::cout << "  解析结果: 年=" << year << ", 月=" << month << ", 日=" << day
        //   << ", 时=" << hour << ", 分=" << minute << ", 秒=" << second << std::endl;

        year += 2000; // 假设是2000年后
        // std::cout << "  调整后年份: " << year << std::endl;

        std::stringstream ss;
        ss << std::setfill('0') << std::setw(4) << year << "-"
           << std::setw(2) << month << "-"
           << std::setw(2) << day << " "
           << std::setw(2) << hour << ":"
           << std::setw(2) << minute << ":"
           << std::setw(2) << second;
        return ss.str();
    }

    double parseSpeed(const uint8_t *data)
    {
        // 3字节速度数据（低字节在前）
        uint32_t speedValue = data[0] | (data[1] << 8) | (data[2] << 16);
        uint16_t actualSpeed = speedValue & 0x3FF; // 取低10位
        return actualSpeed;                        // 移除不必要的0.1倍数
    }

    double parseKilometerPost(const uint8_t *data)
    {
        // 3字节公里标数据（低字节在前）
        uint32_t kmValue = data[0] | (data[1] << 8) | (data[2] << 16);

        bool isNegative = (kmValue & 0x800000) != 0;   // b23: 符号位
        bool isIncreasing = (kmValue & 0x400000) != 0; // b22: 递增/递减
        uint32_t absoluteValue = kmValue & 0x3FFFFF;   // b21～b0: 绝对值

        double kmPost = absoluteValue / 1000.0; // 米转公里

        if (isNegative)
        {
            kmPost = -kmPost;
        }

        return kmPost;
    }
};

// ========== TCPServer类实现 ==========

TCPServer::TCPServer() : serverSocket_(INVALID_SOCKET), serverPort_(0)
{
    // 初始化 Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        throw std::runtime_error("Winsock 初始化失败");
    }
}

TCPServer::~TCPServer()
{
    stopServer();
    WSACleanup();
}

bool TCPServer::startServer(int port)
{
    std::cout << "[LocationReporter] Starting Server" << std::endl;
    // 如果服务器已在运行，先停止
    stopServer();

    serverPort_ = port;

    // 创建TCP监听套接字
    serverSocket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (serverSocket_ == INVALID_SOCKET)
    {
        std::cerr << "创建TCP监听套接字失败，错误码: " << WSAGetLastError() << std::endl;
        return false;
    }

    // 设置地址重用选项
    int reuse = 1;
    if (setsockopt(serverSocket_, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse, sizeof(reuse)) == SOCKET_ERROR)
    {
        std::cerr << "设置套接字选项失败，错误码: " << WSAGetLastError() << std::endl;
    }

    // 绑定到本地地址和端口
    sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY; // 监听所有网络接口
    serverAddr.sin_port = htons(port);

    if (bind(serverSocket_, (sockaddr *)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
    {
        std::cerr << "绑定端口失败，错误码: " << WSAGetLastError() << std::endl;
        closesocket(serverSocket_);
        serverSocket_ = INVALID_SOCKET;
        return false;
    }

    // 开始监听连接
    if (listen(serverSocket_, SOMAXCONN) == SOCKET_ERROR)
    {
        std::cerr << "监听失败，错误码: " << WSAGetLastError() << std::endl;
        closesocket(serverSocket_);
        serverSocket_ = INVALID_SOCKET;
        return false;
    }

    isRunning_ = true;
    std::cout << "[LocationReporter]TCP服务器启动成功，监听端口: " << port << std::endl;

    // 启动监听线程
    listenThread_ = std::thread(&TCPServer::serverListenTask, this);

    return true;
}

void TCPServer::stopServer()
{
    if (isRunning_)
    {
        isRunning_ = false;

        // 关闭监听套接字，这会使accept()返回错误
        if (serverSocket_ != INVALID_SOCKET)
        {
            closesocket(serverSocket_);
            serverSocket_ = INVALID_SOCKET;
        }

        // 等待监听线程结束
        if (listenThread_.joinable())
        {
            listenThread_.join();
        }

        // 关闭所有客户端连接
        {
            std::lock_guard<std::mutex> lock(clientsMutex_);
            for (SOCKET clientSocket : clientSockets_)
            {
                closesocket(clientSocket);
            }
            clientSockets_.clear();
        }

        // 等待所有客户端处理线程结束
        for (auto &thread : clientThreads_)
        {
            if (thread.joinable())
            {
                thread.join();
            }
        }
        clientThreads_.clear();

        std::cout << "TCP服务器已停止" << std::endl;
    }
}

void TCPServer::serverListenTask()
{
    // std::cout << "[LocationReporter]监听线程启动" << std::endl;
    while (isRunning_)
    {
        // std::cout << "[LocationReporter]监听线程启动1" << std::endl;
        sockaddr_in clientAddr;
        int clientAddrLen = sizeof(clientAddr);
        // std::cout << "[LocationReporter]监听线程启动2" << std::endl;

        SOCKET clientSocket = accept(serverSocket_, (sockaddr *)&clientAddr, &clientAddrLen);
        // std::cout << "clientSocket: " << clientSocket << std::endl;

        if (clientSocket == INVALID_SOCKET)
        {
            if (isRunning_) // 只有在服务器仍在运行时才报告错误
            {
                std::cerr << "接受客户端连接失败，错误码: " << WSAGetLastError() << std::endl;
            }
            continue;
        }

        // 将新客户端添加到列表
        {
            std::lock_guard<std::mutex> lock(clientsMutex_);
            clientSockets_.push_back(clientSocket);
        }

        char clientIP[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &clientAddr.sin_addr, clientIP, INET_ADDRSTRLEN);
        std::cout << "新客户端连接: " << clientIP << ":" << ntohs(clientAddr.sin_port)
                  << "，当前客户端数量: " << clientSockets_.size() << std::endl;

        // 为每个客户端创建一个处理线程
        clientThreads_.emplace_back(&TCPServer::handleClient, this, clientSocket, clientAddr);
    }
}

void TCPServer::handleClient(SOCKET clientSocket, sockaddr_in clientAddr)
{
    char clientIP[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &clientAddr.sin_addr, clientIP, INET_ADDRSTRLEN);

    std::cout << "客户端处理线程启动: " << clientIP << ":" << ntohs(clientAddr.sin_port) << std::endl;

    // 监听客户端断开连接（通过接收数据来检测）
    char buffer[1];
    while (isRunning_)
    {
        int result = recv(clientSocket, buffer, sizeof(buffer), 0);
        if (result <= 0)
        {
            // 客户端断开连接或发生错误
            break;
        }
        // 如果客户端发送数据，可以在这里处理（目前我们只是丢弃）
    }

    // 从客户端列表中移除此套接字
    {
        std::lock_guard<std::mutex> lock(clientsMutex_);
        auto it = std::find(clientSockets_.begin(), clientSockets_.end(), clientSocket);
        if (it != clientSockets_.end())
        {
            clientSockets_.erase(it);
        }
    }

    closesocket(clientSocket);
    std::cout << "客户端断开连接: " << clientIP << ":" << ntohs(clientAddr.sin_port)
              << "，当前客户端数量: " << clientSockets_.size() << std::endl;
}

bool TCPServer::sendData(const std::vector<uint8_t> &data)
{
    std::lock_guard<std::mutex> lock(clientsMutex_);

    if (clientSockets_.empty())
    {
        // std::cout << "没有连接的客户端，无法发送数据" << std::endl;
        return false;
    }

    bool success = false;
    std::vector<SOCKET> disconnectedSockets;

    for (SOCKET clientSocket : clientSockets_)
    {
        int result = send(clientSocket, reinterpret_cast<const char *>(data.data()), data.size(), 0);
        if (result == SOCKET_ERROR)
        {
            int errorCode = WSAGetLastError();
            if (errorCode == WSAECONNRESET || errorCode == WSAENOTCONN || errorCode == WSAESHUTDOWN)
            {
                // 客户端已断开，标记为需要移除
                disconnectedSockets.push_back(clientSocket);
            }
        }
        else
        {
            success = true; // 至少有一个客户端发送成功
        }
    }

    // 移除已断开的客户端
    for (SOCKET disconnectedSocket : disconnectedSockets)
    {
        auto it = std::find(clientSockets_.begin(), clientSockets_.end(), disconnectedSocket);
        if (it != clientSockets_.end())
        {
            clientSockets_.erase(it);
            closesocket(disconnectedSocket);
        }
    }

    // if (success)
    // {
    //     // std::cout << "数据发送成功，发送给 " << (clientSockets_.size()) << " 个客户端，字节数: " << data.size() << std::endl;
    //     // // 调试输出：验证实际发送的数据
    //     // std::cout << "[TCPServer] 实际发送的前20字节: ";
    //     // for (size_t i = 0; i < std::min((size_t)20, data.size()); ++i)
    //     // {
    //     //     std::cout << std::hex << std::uppercase << std::setfill('0') << std::setw(2)
    //     //               << (int)data[i] << " ";
    //     // }
    //     // std::cout << std::dec << std::endl;
    // }

    return success;
}

bool TCPServer::hasClients() const
{
    std::lock_guard<std::mutex> lock(clientsMutex_);
    return !clientSockets_.empty();
}

size_t TCPServer::getClientCount() const
{
    std::lock_guard<std::mutex> lock(clientsMutex_);
    return clientSockets_.size();
}

// ========== LocationReporter类实现 ==========

LocationReporter::LocationReporter(int tcpPort, const ObjectTrackingConfig *config)
    : tcpServerPort_(tcpPort), config_(config),
      simulatedKilometerMarker_(1255), // 初始公里标：125.5公里处
      simulatedTrainNumber_(12345),    // 初始车次号：12345（五位数，与RS422格式一致）
      simulatedSpeed_(800),            // 初始速度：80.0 km/h (单位0.1km/h)
      serialPortOpened_(false),        // 串口初始状态为未打开
      hasValidData_(false)             // 初始无有效数据
{
    // 创建TCP服务器实例
    tcpServer_ = std::make_unique<TCPServer>();

    // 创建RS422接口和协议解析器实例
    rs422Interface_ = std::make_unique<SimpleRS422Interface>();
    protocolParser_ = std::make_unique<SimpleProtocolParser>();

    std::cout << "[LocationReporter] 使用RS422串口通信模式初始化" << std::endl;
}

LocationReporter::~LocationReporter()
{
    shutdown();
}

bool LocationReporter::initialize()
{
    // 1. 先初始化RS422串口（如果不是测试模式）
    if (!openRS422Port())
    {
        std::cerr << "RS422串口初始化失败, 将使用模拟GYK数据" << std::endl;
        // return false;
    }

    // 2. 启动TCP服务器
    if (!tcpServer_->startServer(tcpServerPort_))
    {
        std::cerr << "TCP服务器启动失败" << std::endl;
        closeRS422Port();
        return false;
    }

    std::cout << "[LocationReporter] 初始化成功 - RS422串口和TCP服务器已启动" << std::endl;
    return true;
}

bool LocationReporter::isReady() const
{
    return tcpServer_ && tcpServer_->hasClients();
}

size_t LocationReporter::getClientCount() const
{
    return tcpServer_ ? tcpServer_->getClientCount() : 0;
}

void LocationReporter::shutdown()
{
    // 停止TCP服务器
    if (tcpServer_)
    {
        tcpServer_->stopServer();
    }

    // 关闭RS422串口
    closeRS422Port();
}

bool LocationReporter::openRS422Port()
{
    // 从配置文件获取串口参数，如果没有则使用默认值
    std::string portName = config_ && !config_->rs422Port.portName.empty() ? config_->rs422Port.portName : "COM1";
    int baudRate = config_ ? config_->rs422Port.baudRate : GYKProtocol::BAUD_RATE;

    std::cout << "[LocationReporter] 使用RS422配置: portName=" << portName
              << ", baudRate=" << baudRate << std::endl;

    // 打开RS422串口
    if (!rs422Interface_->openPort(portName, baudRate))
    {
        std::cerr << "[LocationReporter] 打开RS422串口失败: " << rs422Interface_->getLastError() << std::endl;
        serialPortOpened_ = false;
        return false;
    }

    serialPortOpened_ = true;
    std::cout << "[LocationReporter] RS422串口初始化成功" << std::endl;
    return true;
}

void LocationReporter::closeRS422Port()
{
    if (rs422Interface_ && rs422Interface_->isOpen())
    {
        rs422Interface_->closePort();
        serialPortOpened_ = false;
        std::cout << "[LocationReporter] RS422串口已关闭" << std::endl;
    }
}

void LocationReporter::reportLocation(uint8_t camera1_visible, uint8_t camera1_thermal,
                                      uint8_t camera2_visible, uint8_t camera2_thermal)
{
    // 如果没有客户端连接，直接丢弃数据包，不阻塞当前线程
    if (!isReady())
    {
        // 此处可以添加日志，但要注意不要过于频繁地刷屏
        // std::cout << "没有客户端连接，丢弃定位数据包" << std::endl;
        return;
    }

    // --- 从RS422串口读取数据 ---
    uint8_t buffer[512]; // RS422数据缓冲区
    int bytesRead = rs422Interface_->readData(buffer, sizeof(buffer));
    // std::cout << "[LocationReporter] bytesRead: " << bytesRead << std::endl;

    if (bytesRead <= 0)
    {
        // 串口已打开且曾经成功解析过数据，使用上一帧数据保持连续性
        if (serialPortOpened_ && hasValidData_)
        {
            std::cout << "[LocationReporter] RS422读取失败，使用上一帧有效数据保持连续性" << std::endl;
            std::vector<uint8_t> lastValidCanData = convertRS422ToCanFormat(lastValidData_);
            assembleAndSendPacket(lastValidCanData, camera1_visible, camera1_thermal, camera2_visible, camera2_thermal);
            return;
        }
        // 串口未打开或从未成功解析过数据，使用模拟数据
        else
        {
            if (!serialPortOpened_)
            {
                std::cout << "[LocationReporter] 串口未打开，使用模拟GYK数据" << std::endl;
            }
            else
            {
                std::cout << "[LocationReporter] 串口已打开但从未成功解析数据，使用模拟GYK数据" << std::endl;
            }
            std::vector<uint8_t> simulatedCanData = generateSimulatedGYKData();
            assembleAndSendPacket(simulatedCanData, camera1_visible, camera1_thermal, camera2_visible, camera2_thermal);
            return;
        }
    }

    // 输出原始RS422数据（十六进制格式）
    // std::cout << "[LocationReporter] 原始RS422数据 (HEX): ";
    // for (int i = 0; i < (std::min)(bytesRead, 50); ++i) // 只显示前50字节避免输出过长
    // {
    //     std::cout << std::hex << std::uppercase << std::setfill('0') << std::setw(2)
    //               << static_cast<int>(buffer[i]) << " ";
    // }
    // if (bytesRead > 50)
    // {
    //     std::cout << "... (还有 " << (bytesRead - 50) << " 字节)";
    // }
    // std::cout << std::dec << std::endl;

    // 查找完整的GYK协议帧
    for (int i = 0; i <= bytesRead - GYKProtocol::MIN_FRAME_LENGTH; ++i)
    {
        // 检查帧起始标志
        if (buffer[i] == GYKProtocol::FRAME_START_DLE &&
            buffer[i + 1] == GYKProtocol::FRAME_START_STX)
        {
            // 读取信息长度（低字节在前）
            if (i + 3 < bytesRead)
            {
                uint16_t frameLength = buffer[i + 2] << 8 | (buffer[i + 3]);
                int totalFrameLength = frameLength + 6; // 包括起始(2字节)、长度(2字节)、结束标志(2字节)
                // std::cout << "[LocationReporter] frameLength: " << frameLength << ", totalFrameLength: " << totalFrameLength << std::endl;
                // 检查是否有完整帧
                if (i + totalFrameLength <= bytesRead)
                {
                    // 解析GYK协议数据
                    // std::cout << "[LocationReporter]正在解析" << std::endl;
                    ParsedGYKData parsedData = protocolParser_->parseFrame(&buffer[i], totalFrameLength);
                    // std::cout << "[LocationReporter] dataTime =" << parsedData.dateTime <<std::endl;
                    if (parsedData.isValid)
                    {
                        // 将解析的RS422数据转换为48字节CAN格式数据
                        std::vector<uint8_t> canFormatData = convertRS422ToCanFormat(parsedData);
                        // std::cout << "[LocationReporter] 将解析的RS422数据转换为48字节CAN格式数据成功" << std::endl;

                        // 调试：显示转换后的CAN数据
                        // std::cout << "[LocationReporter] 转换后的CAN数据 (48字节): ";
                        // for (size_t i = 0; i < canFormatData.size(); ++i)
                        // {
                        //     std::cout << std::hex << std::uppercase << std::setfill('0') << std::setw(2)
                        //               << static_cast<int>(canFormatData[i]);
                        //     if (i < canFormatData.size() - 1)
                        //         std::cout << " ";
                        // }
                        // std::cout << std::dec << std::endl;

                        // 调试：显示CAN数据的关键字段（每100次输出一次）
                        static int debugCount = 0;
                        if (++debugCount % 2000 == 0)
                        {
                            std::cout << "[LocationReporter] CAN数据关键字段解析 (第" << debugCount << "次):" << std::endl;
                            if (canFormatData.size() >= 6)
                            {
                                std::cout << "  时间: " << (int)canFormatData[0] << "-" << (int)canFormatData[1] << "-"
                                          << (int)canFormatData[2] << " " << (int)canFormatData[3] << ":"
                                          << (int)canFormatData[4] << ":" << (int)canFormatData[5] << std::endl;
                            }
                            if (canFormatData.size() >= 10)
                            {
                                uint32_t trainNum = (canFormatData[6] << 24) | (canFormatData[7] << 16) |
                                                    (canFormatData[8] << 8) | canFormatData[9];
                                std::cout << "  车次号: " << trainNum << std::endl;
                            }
                            if (canFormatData.size() >= 18)
                            {
                                uint32_t kmData = (canFormatData[14] << 24) | (canFormatData[15] << 16) |
                                                  (canFormatData[16] << 8) | canFormatData[17];
                                double kmPost = (kmData & 0x3FFFFF) / 1000.0; // 取低22位，转换为公里
                                std::cout << "  公里标: " << kmPost << " km" << std::endl;
                            }
                            if (canFormatData.size() >= 20)
                            {
                                uint16_t speed = (canFormatData[18] << 8) | canFormatData[19];
                                std::cout << "  速度: " << speed << " km/h" << std::endl;
                            }
                        }

                        // 保存有效数据用于数据连续性
                        lastValidData_ = parsedData;
                        hasValidData_ = true;
                        // std::cout << "[LocationReporter] 成功解析并保存有效数据，后续将使用此数据保持连续性" << std::endl;

                        assembleAndSendPacket(canFormatData, camera1_visible, camera1_thermal, camera2_visible, camera2_thermal);
                        std::cout << "[LocationReporter] 发送成功" << std::endl;
                        return; // 处理完一帧后返回
                    }
                    else
                    {
                        // 解析失败，如果曾经成功解析过数据则使用上一帧数据
                        if (hasValidData_)
                        {
                            std::cout << "[LocationReporter] RS422数据解析失败，使用上一帧有效数据保持连续性" << std::endl;
                            std::vector<uint8_t> lastValidCanData = convertRS422ToCanFormat(lastValidData_);
                            assembleAndSendPacket(lastValidCanData, camera1_visible, camera1_thermal, camera2_visible, camera2_thermal);
                            return;
                        }
                        else
                        {
                            std::cout << "[LocationReporter] RS422数据解析失败且从未成功解析过数据，使用模拟GYK数据" << std::endl;
                        }
                    }
                }
            }
        }
    }

    // 如果没有找到有效的GYK帧，根据是否曾经成功解析过数据来选择数据源
    if (hasValidData_)
    {
        std::cout << "[LocationReporter] 未找到有效GYK帧，使用上一帧有效数据保持连续性" << std::endl;
        std::vector<uint8_t> lastValidCanData = convertRS422ToCanFormat(lastValidData_);
        assembleAndSendPacket(lastValidCanData, camera1_visible, camera1_thermal, camera2_visible, camera2_thermal);
    }
    else
    {
        std::cout << "[LocationReporter] 未找到有效GYK帧且从未成功解析过数据，使用模拟GYK数据" << std::endl;
        std::vector<uint8_t> simulatedCanData = generateSimulatedGYKData();
        assembleAndSendPacket(simulatedCanData, camera1_visible, camera1_thermal, camera2_visible, camera2_thermal);
    }
}

void LocationReporter::assembleAndSendPacket(const std::vector<uint8_t> &canPayload,
                                             uint8_t camera1_visible, uint8_t camera1_thermal,
                                             uint8_t camera2_visible, uint8_t camera2_thermal)
{
    std::vector<uint8_t> packetData;
    packetData.reserve(64); // 预分配内存以提高效率

    // 1. 添加报头
    packetData.push_back(0xAA);
    // 2. 在报头后插入4字节检测状态标志位
    packetData.insert(packetData.end(), {camera1_visible, camera1_thermal, camera2_visible, camera2_thermal});
    // 3. 插入CAN数据负载
    packetData.insert(packetData.end(), canPayload.begin(), canPayload.end());
    // 4. 计算CRC校验（覆盖报头到CAN数据的部分）
    uint16_t crc = calculateCRC16(packetData);
    packetData.push_back(static_cast<uint8_t>(crc & 0xFF));
    packetData.push_back(static_cast<uint8_t>((crc >> 8) & 0xFF));
    // 5. 添加报尾
    packetData.push_back(0xFF);

    // 6. 调试：显示最终发送的完整数据包  暂时不显示
    // std::cout << "[LocationReporter] 最终发送的数据包 (完整): ";
    // for (size_t i = 0; i < packetData.size(); ++i)
    // {
    // std::cout << std::hex << std::uppercase << std::setfill('0') << std::setw(2)
    //           << static_cast<int>(packetData[i]);
    // if (i < packetData.size() - 1)
    //     std::cout << " ";
    //}
    // std::cout << std::dec << std::endl;

    // std::cout << "[LocationReporter] 数据包结构分析:" << std::endl;
    // std::cout << "  报头: 0x" << std::hex << std::uppercase << std::setfill('0') << std::setw(2)
    //           << static_cast<int>(packetData[0]) << std::dec << std::endl;
    // std::cout << "  检测标志: [" << (int)packetData[1] << ", " << (int)packetData[2]
    //           << ", " << (int)packetData[3] << ", " << (int)packetData[4] << "]" << std::endl;
    // std::cout << "  CAN数据长度: " << (packetData.size() - 7) << " 字节" << std::endl; // 减去报头(1) + 标志(4) + CRC(2)
    // if (packetData.size() >= 7)
    // {
    // uint16_t crc = (packetData[packetData.size() - 2] << 8) | packetData[packetData.size() - 3];
    // std::cout << "  CRC16: 0x" << std::hex << std::uppercase << std::setfill('0') << std::setw(4)
    //           << crc << std::dec << std::endl;
    // }
    // std::cout << "  报尾: 0x" << std::hex << std::uppercase << std::setfill('0') << std::setw(2)
    //   << static_cast<int>(packetData[packetData.size() - 1]) << std::dec << std::endl;

    // 7. 通过 TCP 服务器广播数据给所有连接的客户端
    bool sendResult = tcpServer_->sendData(packetData);

    // 8. 保存发送的数据包到文件（用于调试和前后端对比）
    // savePacketToFile(packetData, camera1_visible, camera1_thermal, camera2_visible, camera2_thermal, sendResult);
}

uint16_t LocationReporter::calculateCRC16(const std::vector<uint8_t> &data)
{
    uint16_t crc = 0xFFFF; // 初始值
    for (const auto &byte : data)
    {
        crc ^= byte;
        for (int i = 0; i < 8; ++i)
        {
            if (crc & 0x0001)
                crc = (crc >> 1) ^ 0xA001; // 多项式 0xA001
            else
                crc >>= 1;
        }
    }
    return crc;
}

std::vector<uint8_t> LocationReporter::generateSimulatedGYKData()
{
    // 使用固定的原始GYK协议数据进行解析测试
    // 数据来源：SaveWindows2025_8_18_16-11-01.TXT 文件中的一帧数据
    std::string hexData = "10 02 00 50 11 00 01 00 05 00 38 00 67 01 00 01 20 20 20 20 00 00 00 00 00 08 00 00 00 00 00 00 00 00 00 00 00 08 84 D7 00 74 39 C0 05 54 02 27 66 2D 00 00 06 02 FF FF 02 3C 09 00 20 03 2D 00 03 08 84 D7 00 91 9F 12 25 15 01 00 01 00 00 01 00 1F CF 1E 10 03";

    // std::cout << "[LocationReporter] 使用模拟GYK数据: " << hexData << std::endl;

    // 将十六进制字符串转换为字节数组
    std::vector<uint8_t> rawGYKData;
    std::istringstream iss(hexData);
    std::string hexByte;

    while (iss >> hexByte)
    {
        try
        {
            uint8_t byte = static_cast<uint8_t>(std::stoul(hexByte, nullptr, 16));
            rawGYKData.push_back(byte);
        }
        catch (const std::exception &e)
        {
            std::cerr << "[LocationReporter] 解析十六进制数据失败: " << hexByte << ", 错误: " << e.what() << std::endl;
        }
    }

    // std::cout << "[LocationReporter] 转换后的GYK数据长度: " << rawGYKData.size() << " 字节" << std::endl;
    // std::cout << "[LocationReporter] GYK数据 (HEX): ";
    // for (size_t i = 0; i < rawGYKData.size(); ++i)
    // {
    //     std::cout << std::hex << std::uppercase << std::setfill('0') << std::setw(2)
    //               << static_cast<int>(rawGYKData[i]) << " ";
    // }
    // std::cout << std::dec << std::endl;

    // 使用SimpleProtocolParser解析GYK数据
    ParsedGYKData parsedData = protocolParser_->parseFrame(rawGYKData.data(), rawGYKData.size());

    // // 输出解析结果用于调试
    // std::cout << "[LocationReporter] ========== GYK数据解析结果 ==========" << std::endl;
    // std::cout << "[LocationReporter] 解析状态: " << (parsedData.isValid ? "成功" : "失败") << std::endl;
    // if (parsedData.isValid)
    // {
    //     std::cout << "[LocationReporter] 时间: " << parsedData.dateTime << std::endl;
    //     std::cout << "[LocationReporter] 实际速度: " << parsedData.actualSpeed << " km/h" << std::endl;
    //     std::cout << "[LocationReporter] 公里标: " << parsedData.kilometerPost << " km" << std::endl;
    //     std::cout << "[LocationReporter] 五位车次: " << parsedData.fiveDigitTrainNumber << std::endl;
    //     std::cout << "[LocationReporter] 机车号: " << parsedData.locomotiveNumber << std::endl;
    // }
    // else
    // {
    //     std::cout << "[LocationReporter] 解析失败，将使用默认值" << std::endl;
    // }
    // std::cout << "[LocationReporter] =======================================" << std::endl;

    // 将解析后的数据转换为48字节CAN格式
    std::vector<uint8_t> canData;
    if (parsedData.isValid)
    {
        canData = convertRS422ToCanFormat(parsedData);
        // std::cout << "[LocationReporter] 使用解析后的GYK数据生成CAN格式数据" << std::endl;
    }
    else
    {
        // 如果解析失败，使用默认的CAN数据
        canData.resize(48, 0x00);
        std::cout << "[LocationReporter] 解析失败，使用默认CAN数据" << std::endl;
    }

    return canData;
}

std::vector<uint8_t> LocationReporter::convertRS422ToCanFormat(const ParsedGYKData &parsedData)
{
    std::vector<uint8_t> canData(48, 0x00); // 初始化48字节为0

    // ===== 位置5~10：当前时间（年-月-日-时-分-秒，十六进制）=====
    // 解析RS422的时间字符串格式：YYYY-MM-DD HH:MM:SS
    if (!parsedData.dateTime.empty())
    {
        try
        {
            // 简单解析时间字符串
            int year = std::stoi(parsedData.dateTime.substr(0, 4));
            int month = std::stoi(parsedData.dateTime.substr(5, 2));
            int day = std::stoi(parsedData.dateTime.substr(8, 2));
            int hour = std::stoi(parsedData.dateTime.substr(11, 2));
            int minute = std::stoi(parsedData.dateTime.substr(14, 2));
            int second = std::stoi(parsedData.dateTime.substr(17, 2));

            canData[0] = static_cast<uint8_t>(year - 2000); // 年份后两位
            canData[1] = static_cast<uint8_t>(month);       // 月份（1-12）
            canData[2] = static_cast<uint8_t>(day);         // 日期（1-31）
            canData[3] = static_cast<uint8_t>(hour);        // 小时（0-23）
            canData[4] = static_cast<uint8_t>(minute);      // 分钟（0-59）
            canData[5] = static_cast<uint8_t>(second);      // 秒钟（0-59）
        }
        catch (const std::exception &e)
        {
            // 时间解析失败，使用当前系统时间
            auto now = std::chrono::system_clock::now();
            auto time_t = std::chrono::system_clock::to_time_t(now);
            auto tm = *std::localtime(&time_t);

            canData[0] = static_cast<uint8_t>(tm.tm_year + 1900 - 2000);
            canData[1] = static_cast<uint8_t>(tm.tm_mon + 1);
            canData[2] = static_cast<uint8_t>(tm.tm_mday);
            canData[3] = static_cast<uint8_t>(tm.tm_hour);
            canData[4] = static_cast<uint8_t>(tm.tm_min);
            canData[5] = static_cast<uint8_t>(tm.tm_sec);
        }
    }

    // ===== 位置11~14：车次号（4字节）=====
    // 使用RS422解析的五位车次
    if (!parsedData.fiveDigitTrainNumber.empty())
    {
        try
        {
            uint32_t trainNumber = std::stoul(parsedData.fiveDigitTrainNumber);
            canData[6] = static_cast<uint8_t>((trainNumber >> 24) & 0xFF);
            canData[7] = static_cast<uint8_t>((trainNumber >> 16) & 0xFF);
            canData[8] = static_cast<uint8_t>((trainNumber >> 8) & 0xFF);
            canData[9] = static_cast<uint8_t>(trainNumber & 0xFF);
        }
        catch (const std::exception &e)
        {
            // 解析失败，使用默认值
            uint32_t defaultTrain = 1001;
            canData[6] = static_cast<uint8_t>((defaultTrain >> 24) & 0xFF);
            canData[7] = static_cast<uint8_t>((defaultTrain >> 16) & 0xFF);
            canData[8] = static_cast<uint8_t>((defaultTrain >> 8) & 0xFF);
            canData[9] = static_cast<uint8_t>(defaultTrain & 0xFF);
        }
    }

    // ===== 位置15~18：车号（4字节）=====
    // 使用机车号信息填充车号字段
    canData[10] = 0x03; // 车号长度：3个字符
    if (!parsedData.locomotiveNumber.empty())
    {
        // 将机车号的前3个字符填入
        std::string locoStr = parsedData.locomotiveNumber;
        for (int i = 0; i < 3 && i < locoStr.length(); ++i)
        {
            canData[11 + i] = static_cast<uint8_t>(locoStr[i]);
        }
    }
    else
    {
        canData[11] = 'C'; // 默认车号：CRH
        canData[12] = 'R';
        canData[13] = 'H';
    }

    // ===== 位置19~22：公里标（4字节）=====
    // 使用RS422解析的公里标数据
    uint32_t kmMarker = static_cast<uint32_t>(parsedData.kilometerPost * 1000); // 公里转米
    uint32_t kmData = kmMarker | (1 << 23);                                     // 设置趋势位为增加

    // 调试：输出公里标设置过程
    // std::cout << "[LocationReporter] 公里标设置调试:" << std::endl;
    // std::cout << "  原始公里标: " << parsedData.kilometerPost << " km" << std::endl;
    // std::cout << "  转换为米: " << kmMarker << " m" << std::endl;
    // std::cout << "  加上趋势位: 0x" << std::hex << std::uppercase << kmData << std::dec << std::endl;

    canData[14] = static_cast<uint8_t>((kmData >> 24) & 0xFF);
    canData[15] = static_cast<uint8_t>((kmData >> 16) & 0xFF);
    canData[16] = static_cast<uint8_t>((kmData >> 8) & 0xFF);
    canData[17] = static_cast<uint8_t>(kmData & 0xFF);

    // std::cout << "  设置的字节: 0x" << std::hex << std::uppercase
    //           << std::setfill('0') << std::setw(2) << static_cast<int>(canData[14]) << " "
    //           << std::setw(2) << static_cast<int>(canData[15]) << " "
    //           << std::setw(2) << static_cast<int>(canData[16]) << " "
    //           << std::setw(2) << static_cast<int>(canData[17]) << std::dec << std::endl;

    // ===== 位置23~24：速度（2字节，单位0.1km/h）=====
    // 使用RS422解析的实速数据
    uint16_t speed = static_cast<uint16_t>(parsedData.actualSpeed); // 直接使用km/h单位
    canData[18] = static_cast<uint8_t>((speed >> 8) & 0xFF);
    canData[19] = static_cast<uint8_t>(speed & 0xFF);

    // ===== 位置25~52：其他数据简单填充 =====
    canData[20] = 0x01;
    canData[21] = 0x05; // 车站号：0x0105
    canData[22] = 0x02;
    canData[23] = 0x01; // 线路号：0x0201
    canData[24] = 0x04;
    canData[25] = 0xB0; // 限速：120.0 km/h (0x04B0 = 1200)
    canData[26] = 0x05; // 行别：上行主线正向（B2=1, B1=0, B0=1）
    canData[27] = 0x01; // 交路号：1
    canData[28] = 0x01; // 机车信号：绿灯

    // 司机号（3字节）
    canData[29] = 0x12;
    canData[30] = 0x34;
    canData[31] = 0x56;
    // 副司机号（3字节）
    canData[32] = 0x78;
    canData[33] = 0x9A;
    canData[34] = 0xBC;
    // 曲线半径（3字节，单位：米）
    canData[35] = 0x00;
    canData[36] = 0x03;
    canData[37] = 0xE8; // 1000米
    // 坡度（2字节）
    canData[38] = 0x00;
    canData[39] = 0x0A; // 10‰

    // 预留字段（位置45~52，即索引40~47）
    for (int i = 40; i < 48; ++i)
    {
        canData[i] = 0x00;
    }

    return canData;
}

void LocationReporter::savePacketToFile(const std::vector<uint8_t> &packetData,
                                        uint8_t camera1_visible, uint8_t camera1_thermal,
                                        uint8_t camera2_visible, uint8_t camera2_thermal,
                                        bool sendResult) const
{
    try
    {
        // 只有在实际发送数据包或者有检测标志位为1时才记录
        if (!sendResult && camera1_visible == 0 && camera1_thermal == 0 &&
            camera2_visible == 0 && camera2_thermal == 0)
        {
            return; // 跳过无意义的记录
        }

        // 打开文件（追加模式）
        std::ofstream file("sent_packets_log.txt", std::ios::app);
        if (!file.is_open())
        {
            std::cerr << "[LocationReporter] 无法打开数据包日志文件" << std::endl;
            return;
        }

        // 获取当前时间戳
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                      now.time_since_epoch()) %
                  1000;

        std::stringstream timeStr;
        timeStr << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
        timeStr << "." << std::setfill('0') << std::setw(3) << ms.count();

        // 写入时间戳和基本信息
        file << "======[LocationReporter] 写入时间戳和基本信息=====" << std::endl;
        file << "Time: " << timeStr.str() << std::endl;
        file << "Detection Flags: camera1_visible=" << (int)camera1_visible
             << ", camera1_thermal=" << (int)camera1_thermal
             << ", camera2_visible=" << (int)camera2_visible
             << ", camera2_thermal=" << (int)camera2_thermal << std::endl;
        file << "Send Result: " << (sendResult ? "SUCCESS" : "FAILED") << std::endl;
        file << "Packet Size: " << packetData.size() << " bytes" << std::endl;
        file << "Client Count: " << getClientCount() << std::endl;

        // 写入完整的数据包内容（十六进制格式）
        file << "Packet Data (HEX): ";
        for (size_t i = 0; i < packetData.size(); ++i)
        {
            file << std::hex << std::uppercase << std::setfill('0') << std::setw(2)
                 << (int)packetData[i];
            if (i < packetData.size() - 1)
                file << " ";
        }
        file << std::dec << std::endl;

        // 分段解析数据包结构
        file << "Packet Structure Analysis:" << std::endl;
        if (packetData.size() > 0)
        {
            file << "  Header: 0x" << std::hex << std::uppercase << std::setfill('0')
                 << std::setw(2) << (int)packetData[0] << std::dec << std::endl;
        }
        if (packetData.size() >= 5)
        {
            file << "  Detection Flags: [" << (int)packetData[1] << ", "
                 << (int)packetData[2] << ", " << (int)packetData[3] << ", "
                 << (int)packetData[4] << "]" << std::endl;
        }
        if (packetData.size() >= 53)
        { // 5 + 48 CAN数据
            file << "  CAN Data (48 bytes): ";
            for (int i = 5; i < 53 && i < packetData.size(); ++i)
            {
                file << std::hex << std::uppercase << std::setfill('0') << std::setw(2)
                     << (int)packetData[i];
                if (i < 52)
                    file << " ";
            }
            file << std::dec << std::endl;
        }
        if (packetData.size() >= 55)
        { // CRC 2字节
            uint16_t crc = (packetData[54] << 8) | packetData[53];
            file << "  CRC16: 0x" << std::hex << std::uppercase << std::setfill('0')
                 << std::setw(4) << crc << std::dec << std::endl;
        }
        if (packetData.size() >= 56)
        { // 报尾
            file << "  Footer: 0x" << std::hex << std::uppercase << std::setfill('0')
                 << std::setw(2) << (int)packetData[packetData.size() - 1] << std::dec << std::endl;
        }

        file << std::endl;
        file.close();

        // 输出控制台信息（仅在有检测标志位时）
        if (camera1_visible || camera1_thermal || camera2_visible || camera2_thermal)
        {
            std::cout << "[LocationReporter] 数据包已保存到 sent_packets_log.txt, 大小: "
                      << packetData.size() << " bytes, 发送: "
                      << (sendResult ? "成功" : "失败") << std::endl;
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "[LocationReporter] 保存数据包日志时发生错误: " << e.what() << std::endl;
    }
}