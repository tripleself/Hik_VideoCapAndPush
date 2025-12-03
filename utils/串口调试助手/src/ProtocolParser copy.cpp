#include "ProtocolParser.h"
#include <sstream>
#include <iomanip>
#include <iostream>

ProtocolParser::ProtocolParser()
{
}

ProtocolParser::ParsedData ProtocolParser::parseFrame(const uint8_t *data, size_t length)
{
    ParsedData result;

    // 验证数据帧基本格式
    if (!validateFrame(data, length))
    {
        std::cout << "数据帧验证失败" << std::endl;
        return result;
    }

    try
    {
        // 根据协议文档解析各个字段
        // 帧起始：DLE(10H), STX(02H) - 位置0-1
        // 信息长度：2字节 - 位置2-3 (低字节在前)

        // 调试：输出数据包的关键位置
        std::cout << "=== 数据包分析 ===" << std::endl;
        std::cout << "帧起始: " << std::hex << (int)data[0] << " " << (int)data[1] << std::dec << std::endl;
        std::cout << "信息长度: " << (data[2] << 8 | (data[3])) << " 字节" << std::endl;

        // 解析车次数字部分（位置22，3字节，低在前）- 根据协议文档调整位置
        if (length > 24)//待改
        {
            // 车次数字部分在位置22（从0开始计数）
            uint32_t trainNum = data[22] | (data[23] << 8) | (data[24] << 16);
            result.trainNumber = std::to_string(trainNum);
        }

        for (size_t i = 0; i <= 3; i++)
        {
            std::cout << "找到时间数据在位置 " << i << ": 5A 02 27 66" << std::endl;
            result.dateTime = parseBCDTime(&data[45 + i]);
        }

        // 解析实速（位置28，3字节，低在前）

        for (size_t i = 0; i <= 2; i++)
        {
            // std::cout << "找到时间数据在位置 " << i << ": 5A 02 27 66" << std::endl;
            result.actualSpeed = parseSpeed(&data[49 + i]);
        }


        // 解析公里标（位置33，3字节）
        for (size_t i = 0; i <= 2; i++)
        {
            result.kilometerPost = parseKilometerPost(&data[57 + i]);
        }

        // 解析机车号（位置43，2字节，低在前）
        for (size_t i = 0; i <= 1; i++)
        {
            result.locomotiveNumber = parseLocomotiveNumber(&data[74 + i]);
        }

        // 解析车号补充（位置12，1字节）- 根据补充帧
        if (length > 12)
        {
            result.carNumberSupplement = std::to_string(data[12]);
        }

        result.isValid = true;
        std::cout << "数据解析成功 - 车次:" << result.trainNumber
                  << " 时间:" << result.dateTime
                  << " 速度:" << result.actualSpeed << "km/h" << std::endl;
    }
    catch (const std::exception &e)
    {
        std::cout << "解析异常: " << e.what() << std::endl;
        result.isValid = false;
    }

    return result;
}

std::string ProtocolParser::toJsonString(const ParsedData &data)
{
    std::stringstream ss;
    ss << "{\n";
    ss << "  \"trainNumber\": \"" << data.trainNumber << "\",\n";
    ss << "  \"dateTime\": \"" << data.dateTime << "\",\n";
    ss << "  \"actualSpeed\": " << std::fixed << std::setprecision(1) << data.actualSpeed << ",\n";
    ss << "  \"kilometerPost\": " << std::fixed << std::setprecision(3) << data.kilometerPost << ",\n";
    ss << "  \"locomotiveNumber\": \"" << data.locomotiveNumber << "\",\n";
    ss << "  \"carNumberSupplement\": \"" << data.carNumberSupplement << "\",\n";
    ss << "  \"isValid\": " << (data.isValid ? "true" : "false") << "\n";
    ss << "}";
    return ss.str();
}

bool ProtocolParser::validateFrame(const uint8_t *data, size_t length)
{
    // 检查最小长度
    if (length < 10)
    {
        std::cout << "数据长度不足: " << length << std::endl;
        return false;
    }

    // 检查帧起始标志：DLE(10H), STX(02H)
    if (data[0] != 0x10 || data[1] != 0x02)
    {
        std::cout << "帧起始标志错误: " << std::hex << (int)data[0] << " " << (int)data[1] << std::dec << std::endl;
        return false;
    }

    // 检查信息长度 (低字节在前)
    uint16_t frameLength = data[2] << 8 | (data[3]);
    std::cout << "帧长度: " << frameLength << ", 实际长度: " << length << std::endl;

    // 对于实际数据，可能包含额外的时间戳等信息，所以放宽长度检查
    if (length != frameLength + 6)
    {
        std::cout << "数据长度不匹配预期帧长度" << std::endl;
        // 不直接返回false，继续尝试解析
    }

    // 检查帧结束标志：DLE(10H), ETX(03H)
    if (length == frameLength + 6)
    {
        size_t endPos = frameLength + 4;
        if (data[endPos] == 0x10 && data[endPos + 1] == 0x03)
        {
            std::cout << "找到帧结束标志" << std::endl;
        }
        else
        {
            std::cout << "帧结束标志不匹配" << std::endl;
        }
    }

    return true;
}

uint16_t ProtocolParser::calculateCRC(const uint8_t *data, size_t length)
{
    // 简单的CRC16计算（根据实际协议调整）
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < length; i++)
    {
        crc ^= data[i];
        for (int j = 0; j < 8; j++)
        {
            if (crc & 1)
            {
                crc = (crc >> 1) ^ 0xA001;
            }
            else
            {
                crc >>= 1;
            }
        }
    }
    return crc;
}

std::string ProtocolParser::parseBCDTime(const uint8_t *data)
{
    // 根据协议文档：b5～b0：秒，b11～b6：分，b16～b12：时，b21～b17：日，b25～b22：月，b26～b31:年（低在前）
    // 4字节数据，低字节在前
    uint32_t timeValue = data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);

    int second = timeValue & 0x3F;        // b5～b0：秒
    int minute = (timeValue >> 6) & 0x3F; // b11～b6：分
    int hour = (timeValue >> 12) & 0x1F;  // b16～b12：时
    int day = (timeValue >> 17) & 0x1F;   // b21～b17：日
    int month = (timeValue >> 22) & 0x0F; // b25～b22：月
    int year = (timeValue >> 26) & 0x3F;  // b31～b26：年

    // 年份处理：假设是2000年后的年份
    year += 2000;

    // 调试信息：显示解析过程
    std::cout << "时间解析调试: 原始字节=[" << std::hex
              << (int)data[0] << " " << (int)data[1] << " "
              << (int)data[2] << " " << (int)data[3] << "]" << std::dec << std::endl;
    std::cout << "组合值=0x" << std::hex << timeValue << std::dec
              << " -> " << year << "-" << month << "-" << day
              << " " << hour << ":" << minute << ":" << second << std::endl;

    std::stringstream ss;
    ss << std::setfill('0') << std::setw(4) << year << "-"
       << std::setw(2) << month << "-"
       << std::setw(2) << day << " "
       << std::setw(2) << hour << ":"
       << std::setw(2) << minute << ":"
       << std::setw(2) << second;
    return ss.str();
}

uint32_t ProtocolParser::parseBCDValue(const uint8_t *data, int bytes)
{
    uint32_t result = 0;
    for (int i = 0; i < bytes; i++)
    {
        uint8_t byte = data[i];
        uint8_t high = (byte >> 4) & 0x0F;
        uint8_t low = byte & 0x0F;
        result = result * 100 + high * 10 + low;
    }
    return result;
}

double ProtocolParser::parseSpeed(const uint8_t *data)
{
    // 解析3字节速度数据（低字节在前）
    // 根据协议文档：b9～b0：实速，b19～b10：预留 （低在前）
    uint32_t speedValue = data[0] | (data[1] << 8) | (data[2] << 16);
    // 取低10位作为实速
    uint16_t actualSpeed = speedValue & 0x3FF;
    // 速度单位可能需要根据实际情况调整
    return actualSpeed * 0.1;
}

std::string ProtocolParser::parseLocomotiveNumber(const uint8_t *data)
{
    uint16_t locoNum = data[0] | (data[1] << 8);
    return std::to_string(locoNum);
}

double ProtocolParser::parseKilometerPost(const uint8_t *data)
{
    // 解析3字节公里标数据（低字节在前）
    // 根据协议：单位:米。b23:符号位（0表示正，1表示负）,b22:符号位（0表示递减，1表示递增），b21～b0:公里标绝对值
    uint32_t kmValue = data[0] | (data[1] << 8) | (data[2] << 16);

    // 提取符号位和绝对值
    bool isNegative = (kmValue & 0x800000) != 0;   // b23: 符号位
    bool isIncreasing = (kmValue & 0x400000) != 0; // b22: 递增/递减
    uint32_t absoluteValue = kmValue & 0x3FFFFF;   // b21～b0: 绝对值

    // 转换为公里（米转公里）
    double kmPost = absoluteValue / 1000.0;

    // 应用符号
    if (isNegative)
    {
        kmPost = -kmPost;
    }

    return kmPost;
}
