#pragma once
#include <string>
#include <vector>
#include <cstdint>

/**
 * @brief RS422串口通信接口类
 *
 * 提供跨平台的RS422串口通信接口，便于移植到不同的Windows环境
 * 支持真实串口和文件模拟两种模式
 */
class RS422Interface
{
public:
    /**
     * @brief 构造函数
     */
    RS422Interface();

    /**
     * @brief 析构函数
     */
    ~RS422Interface();

    /**
     * @brief 打开串口
     * @param portName 串口名称 (如 "COM1")
     * @param baudRate 波特率 (默认9600)
     * @return 是否成功
     */
    bool openPort(const std::string &portName, int baudRate = 9600);

    /**
     * @brief 关闭串口
     */
    void closePort();

    /**
     * @brief 读取数据
     * @param buffer 数据缓冲区
     * @param maxLength 最大读取长度
     * @return 实际读取的字节数，-1表示错误
     */
    int readData(uint8_t *buffer, int maxLength);

    /**
     * @brief 检查串口是否已打开
     * @return 是否已打开
     */
    bool isOpen() const;

    /**
     * @brief 启用文件模拟模式（用于测试）
     * @param filename 数据文件名
     * @return 是否成功
     */
    bool enableFileSimulation(const std::string &filename);

    /**
     * @brief 获取错误信息
     * @return 错误信息字符串
     */
    std::string getLastError() const;

private:
    void *m_handle;                                     // 串口句柄 (Windows HANDLE)
    bool m_isOpen;                                      // 是否已打开
    bool m_isSimulation;                                // 是否为模拟模式
    std::vector<std::vector<uint8_t>> m_simulationData; // 模拟数据
    size_t m_currentFrame;                              // 当前帧索引
    std::string m_lastError;                            // 最后的错误信息

    /**
     * @brief 解析十六进制字符串为字节数组
     * @param hexStr 十六进制字符串
     * @return 字节数组
     */
    std::vector<uint8_t> parseHexString(const std::string &hexStr);

    /**
     * @brief 加载模拟数据文件
     * @param filename 文件名
     * @return 是否成功
     */
    bool loadSimulationData(const std::string &filename);
};
