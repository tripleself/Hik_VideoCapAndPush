#pragma once

#include <string>
#include <vector>
#include <memory>
#include <stdexcept>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>

// Forward declaration
struct ObjectTrackingConfig;

// Windows网络编程相关头文件
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

// ParsedGYKData结构体定义
struct ParsedGYKData
{
    std::string dateTime;             // 年月日时分秒
    double actualSpeed;               // 实速 km/h
    double kilometerPost;             // 公里标 km
    std::string fiveDigitTrainNumber; // 五位车次
    std::string locomotiveNumber;     // 机车号
    bool isValid;                     // 数据是否有效

    ParsedGYKData() : actualSpeed(0.0), kilometerPost(0.0), isValid(false) {}
};
class SimpleRS422Interface;
class SimpleProtocolParser;

/**
 * @brief TCP网络服务器类，负责通过TCP协议向连接的客户端发送定位数据
 */
class TCPServer
{
public:
    /**
     * @brief 构造函数，初始化Winsock
     */
    TCPServer();

    /**
     * @brief 析构函数，清理网络资源
     */
    ~TCPServer();

    /**
     * @brief 启动TCP服务器，绑定到指定端口并开始监听
     * @param port 本地监听端口号
     * @return true 启动成功, false 启动失败
     */
    bool startServer(int port);

    /**
     * @brief 停止TCP服务器
     */
    void stopServer();

    /**
     * @brief 发送数据到所有连接的客户端
     * @param data 要发送的数据
     * @return true 至少发送给一个客户端成功, false 没有客户端或发送失败
     */
    bool sendData(const std::vector<uint8_t> &data);

    /**
     * @brief 检查是否有客户端连接
     */
    // bool hasClients() const { return !clientSockets_.empty(); }
    bool hasClients() const;

    /**
     * @brief 获取连接的客户端数量
     */
    // size_t getClientCount() const{return clientSockets_.size();}
    size_t getClientCount() const;

private:
    /**
     * @brief 服务器监听线程函数
     */
    void serverListenTask();

    /**
     * @brief 处理客户端连接的线程函数
     * @param clientSocket 客户端套接字
     * @param clientAddr 客户端地址
     */
    void handleClient(SOCKET clientSocket, sockaddr_in clientAddr);

    SOCKET serverSocket_;                    // 服务器监听套接字
    std::atomic<bool> isRunning_{false};     // 服务器运行状态
    std::thread listenThread_;               // 监听线程
    std::vector<std::thread> clientThreads_; // 客户端处理线程
    std::vector<SOCKET> clientSockets_;      // 已连接的客户端套接字列表
    mutable std::mutex clientsMutex_;        // 保护客户端列表的互斥锁
    int serverPort_;                         // 服务器监听端口
};

/**
 * @brief 定位上报模块，集成RS422串口通信和TCP服务器功能
 * 提供统一的定位数据读取和广播接口
 */
class LocationReporter
{
public:
    /**
     * @brief 构造函数
     * @param tcpPort TCP服务器监听端口号
     * @param config 配置对象，包含RS422串口参数（可选）
     */
    LocationReporter(int tcpPort, const ObjectTrackingConfig *config = nullptr);

    /**
     * @brief 析构函数，确保资源清理
     */
    ~LocationReporter();

    /**
     * @brief 初始化RS422串口和TCP服务器
     * @return 初始化是否成功
     */
    bool initialize();

    /**
     * @brief 执行定位数据读取和广播
     * @param camera1_visible 一位端可见光检测状态 (0=无目标, 1=检测到目标)
     * @param camera1_thermal 一位端热成像检测状态 (0=无目标, 1=检测到目标)
     * @param camera2_visible 二位端可见光检测状态 (0=无目标, 1=检测到目标)
     * @param camera2_thermal 二位端热成像检测状态 (0=无目标, 1=检测到目标)
     */
    void reportLocation(uint8_t camera1_visible = 0, uint8_t camera1_thermal = 0,
                        uint8_t camera2_visible = 0, uint8_t camera2_thermal = 0);

    /**
     * @brief 检查TCP服务器是否就绪
     */
    bool isReady() const;

    /**
     * @brief 获取连接的客户端数量
     */
    size_t getClientCount() const;

private:
    /**
     * @brief 组装包含车辆运行数据的完整数据包并广播
     * @param canPayload 从RS422串口读取或模拟生成的原始数据负载（保持48字节CAN格式兼容）
     * @param camera1_visible 一位端可见光检测状态
     * @param camera1_thermal 一位端热成像检测状态
     * @param camera2_visible 二位端可见光检测状态
     * @param camera2_thermal 二位端热成像检测状态
     */
    void assembleAndSendPacket(const std::vector<uint8_t> &canPayload,
                               uint8_t camera1_visible, uint8_t camera1_thermal,
                               uint8_t camera2_visible, uint8_t camera2_thermal);

    /**
     * @brief 安全关闭所有连接和线程
     */
    void shutdown();

    // RS422串口通信相关
    std::unique_ptr<SimpleRS422Interface> rs422Interface_;
    std::unique_ptr<SimpleProtocolParser> protocolParser_;

    // TCP服务器
    std::unique_ptr<TCPServer> tcpServer_;

    // 配置参数
    int tcpServerPort_;
    const ObjectTrackingConfig *config_; // 配置对象指针

    /**
     * @brief 打开RS422串口
     */
    bool openRS422Port();

    /**
     * @brief 关闭RS422串口
     */
    void closeRS422Port();

    /**
     * @brief 将RS422解析的数据转换为48字节CAN格式（保持兼容性）
     * @param parsedData RS422协议解析后的数据
     * @return 48字节的CAN格式数据
     */
    std::vector<uint8_t> convertRS422ToCanFormat(const ParsedGYKData &parsedData);

    /**
     * @brief 计算CRC16校验，与TaskLocating.cpp中算法一致
     */
    uint16_t calculateCRC16(const std::vector<uint8_t> &data);

    /**
     * @brief 生成模拟的GYK协议数据，使用固定的原始十六进制数据进行解析测试
     * @return 48字节的模拟数据负载（保持CAN格式兼容）
     */
    std::vector<uint8_t> generateSimulatedGYKData();

    /**
     * @brief 保存发送的数据包到文件，用于调试和前后端数据对比
     * @param packetData 完整的数据包
     * @param camera1_visible 一位端可见光检测状态
     * @param camera1_thermal 一位端热成像检测状态
     * @param camera2_visible 二位端可见光检测状态
     * @param camera2_thermal 二位端热成像检测状态
     * @param sendResult 发送结果
     */
    void savePacketToFile(const std::vector<uint8_t> &packetData,
                          uint8_t camera1_visible, uint8_t camera1_thermal,
                          uint8_t camera2_visible, uint8_t camera2_thermal,
                          bool sendResult) const;

    // 模拟数据用的计数器和状态变量
    mutable uint32_t simulatedKilometerMarker_; // 模拟公里标（单位：米）
    mutable uint32_t simulatedTrainNumber_;     // 模拟车次号
    mutable uint16_t simulatedSpeed_;           // 模拟速度（单位：0.1km/h）

    // 串口状态和数据连续性管理
    bool serialPortOpened_;               // 串口是否成功打开
    mutable ParsedGYKData lastValidData_; // 保存最后一帧有效数据
    mutable bool hasValidData_;           // 是否有有效数据
};