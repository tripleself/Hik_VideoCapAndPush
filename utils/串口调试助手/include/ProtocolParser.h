#pragma once
#include <vector>
#include <string>
#include <cstdint>

/**
 * @brief GYK公用数据箱输出通讯协议解析器
 *
 * 该类用于解析RS422串口通信协议数据，提取关键信息：
 * - 时间（年月日时分秒）- 序号46，位置45-48
 * - 实速 - 序号50，位置49-51
 * - 公里标 - 序号58，位置57-59
 * - 五位车次 - 序号67，位置66-67
 * - 机车号 - 序号75，位置74-75
 *
 * 模块化设计，便于集成到其他项目中
 */
class ProtocolParser
{
public:
    /**
     * @brief 解析后的数据结构
     * 根据序号重排.md，只包含必要的字段：时间、实速、公里标、五位车次、机车号
     */
    struct ParsedData
    {
        std::string dateTime;             // 年月日时分秒 (序号46，位置45)
        double actualSpeed;               // 实速 km/h (序号50，位置49)
        double kilometerPost;             // 公里标 km (序号58，位置57)
        std::string fiveDigitTrainNumber; // 五位车次 (序号67，位置66)
        std::string locomotiveNumber;     // 机车号 (序号75，位置74)
        bool isValid;                     // 数据是否有效

        ParsedData() : actualSpeed(0.0), kilometerPost(0.0), isValid(false) {}
    };

    /**
     * @brief 构造函数
     */
    ProtocolParser();

    /**
     * @brief 解析数据帧
     * @param data 原始数据
     * @param length 数据长度
     * @return 解析后的数据结构
     */
    ParsedData parseFrame(const uint8_t *data, size_t length);

    /**
     * @brief 将解析后的数据转换为JSON字符串
     * @param data 解析后的数据
     * @return JSON格式字符串
     */
    std::string toJsonString(const ParsedData &data);

private:
    /**
     * @brief 计算CRC校验
     * @param data 数据
     * @param length 长度
     * @return CRC值
     */
    uint16_t calculateCRC(const uint8_t *data, size_t length);

    /**
     * @brief 验证数据帧格式
     * @param data 数据
     * @param length 长度
     * @return 是否有效
     */
    bool validateFrame(const uint8_t *data, size_t length);

    /**
     * @brief 解析BCD码时间
     * @param data 时间数据（4字节）
     * @return 时间字符串 "YYYY-MM-DD HH:MM:SS"
     */
    std::string parseBCDTime(const uint8_t *data);

    /**
     * @brief 解析BCD码数值
     * @param data 数据
     * @param bytes 字节数
     * @return 十进制值
     */
    uint32_t parseBCDValue(const uint8_t *data, int bytes);

    /**
     * @brief 解析速度值
     * @param data 速度数据（3字节）
     * @return 速度值 (km/h)
     */
    double parseSpeed(const uint8_t *data);

    /**
     * @brief 解析公里标
     * @param data 公里标数据（3字节）
     * @return 公里标值
     */
    double parseKilometerPost(const uint8_t *data);

    /**
     * @brief 解析机车号
     * @param data 机车号数据（2字节）
     * @return 机车号字符串
     */
    std::string parseLocomotiveNumber(const uint8_t *data);
};
