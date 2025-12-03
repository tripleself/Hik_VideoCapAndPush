#pragma once

/**
 * @brief GYK协议配置常量
 *
 * 根据序号重排.md文档定义的协议常量，便于移植和维护
 */
namespace GYKProtocol
{

    // 帧标识符
    static const uint8_t FRAME_START_DLE = 0x10;
    static const uint8_t FRAME_START_STX = 0x02;
    static const uint8_t FRAME_END_DLE = 0x10;
    static const uint8_t FRAME_END_ETX = 0x03;

    // 数据字段位置（基于序号重排.md，从0开始计数）
    static const int POS_FRAME_START = 0;        // 帧起始，2字节
    static const int POS_INFO_LENGTH = 2;        // 信息长度，2字节，低字节在前
    static const int POS_DATE_TIME = 45;         // 时间，4字节，序号46
    static const int POS_ACTUAL_SPEED = 49;      // 实速，3字节，序号50
    static const int POS_KILOMETER_POST = 57;    // 公里标，3字节，序号58
    static const int POS_FIVE_DIGIT_TRAIN = 66;  // 五位车次，2字节，序号67
    static const int POS_LOCOMOTIVE_NUMBER = 74; // 机车号，2字节，序号75

    // 数据字段长度
    static const int LEN_FRAME_START = 2;
    static const int LEN_INFO_LENGTH = 2;
    static const int LEN_DATE_TIME = 4;
    static const int LEN_ACTUAL_SPEED = 3;
    static const int LEN_KILOMETER_POST = 3;
    static const int LEN_FIVE_DIGIT_TRAIN = 2;
    static const int LEN_LOCOMOTIVE_NUMBER = 2;

    // 最小帧长度
    static const int MIN_FRAME_LENGTH = 10;

    // 串口通信参数（用于RS422移植）
    static const int BAUD_RATE = 9600;
    static const int DATA_BITS = 8;
    static const int STOP_BITS = 1;
    static const int PARITY_NONE = 0;
}
