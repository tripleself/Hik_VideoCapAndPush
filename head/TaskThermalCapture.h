#pragma once
#include "SharedData.h"
#include "HCNetSDK.h"
#include <thread>
#include <vector>
#include <opencv2/opencv.hpp>
#include <chrono>

/**
 * @brief 热成像数据捕获任务类
 * 负责从热成像视频流通过颜色分析获取温度矩阵数据，支持多设备（1-2个摄像头）
 * 不再使用海康SDK获取温度数据，而是通过分析视频流颜色反推温度数据
 */
class TaskThermalCapture
{
public:
    /**
     * @brief 构造函数，初始化设备用户ID列表和共享数据
     * @param userIDs 海康设备用户ID列表
     * @param data 共享数据引用
     */
    TaskThermalCapture(const std::vector<LONG> &userIDs, SharedData &data);

    /**
     * @brief 析构函数，确保线程安全停止
     */
    ~TaskThermalCapture();

    /**
     * @brief 启动热成像数据捕获线程
     */
    void start();

    /**
     * @brief 停止热成像数据捕获线程
     */
    void stop();

    /**
     * @brief 设置百分位数阈值
     * @param percentile 百分位数值 (0.0-1.0, 如0.8表示80%分位数)
     * @return 是否设置成功
     */
    bool setPercentileThreshold(float percentile);

private:
    /**
     * @brief 线程主函数，循环从热成像视频流分析温度数据
     */
    void run();

    /**
     * @brief 获取缓存的温度范围数据
     * @param deviceIdx 设备索引（0或1）
     * @param minTemp 输出最低温度
     * @param maxTemp 输出最高温度
     * @return 是否获取到有效数据
     */
    bool getCachedTemperatureRange(int deviceIdx, float &minTemp, float &maxTemp);

    /**
     * @brief 一次性初始化调色板并计算阈值灰度值
     * @param frame 热成像视频帧
     * @return 是否初始化成功
     */
    bool initializePalette(const cv::Mat &frame);

    /**
     * @brief 计算向量的指定百分位数值
     * @param values 输入数值向量
     * @param percentile 百分位数 (0.0-1.0)
     * @return 百分位数对应的值
     */
    float calculatePercentile(std::vector<float> &values, float percentile);

    /**
     * @brief 从温度条区域读取调色板颜色范围（已简化）
     * @param frame 热成像视频帧
     * @return 调色板的灰度值向量，从低温到高温排列
     */
    std::vector<float> extractTemperaturePalette(const cv::Mat &frame);

    /**
     * @brief 创建屏蔽区域掩码
     * @param frame 热成像视频帧
     * @return 屏蔽掩码（255=有效区域，0=屏蔽区域）
     */
    cv::Mat createMaskRegions(const cv::Mat &frame);

    /**
     * @brief 根据颜色和温度范围建立线性映射关系
     * @param grayValue 像素灰度值（0-255）
     * @param palette 调色板灰度值向量
     * @param minTemp 最低温度
     * @param maxTemp 最高温度
     * @return 对应的温度值
     */
    float mapGrayToTemperature(float grayValue, const std::vector<float> &palette,
                               float minTemp, float maxTemp);

    /**
     * @brief 从热成像视频帧生成温度矩阵
     * @param frame 热成像视频帧（1280x720）
     * @param minTemp 最低温度
     * @param maxTemp 最高温度
     * @return 温度矩阵（640x512，CV_32FC1）
     */
    cv::Mat generateTemperatureMatrix(const cv::Mat &frame, float minTemp, float maxTemp);

    // 基本成员变量
    std::vector<LONG> userIDs_; // 海康设备用户ID列表
    SharedData &data_;          // 共享数据引用
    std::thread thread_;        // 热成像捕获线程

    // 温度数据缓存
    float cachedMinTemp_1_;                                      // 设备1缓存的最低温度
    float cachedMaxTemp_1_;                                      // 设备1缓存的最高温度
    float cachedMinTemp_2_;                                      // 设备2缓存的最低温度
    float cachedMaxTemp_2_;                                      // 设备2缓存的最高温度
    std::chrono::steady_clock::time_point lastTempUpdateTime_1_; // 设备1温度更新时间
    std::chrono::steady_clock::time_point lastTempUpdateTime_2_; // 设备2温度更新时间

    // 性能监控
    int frameCount_;             // 已处理帧数
    double totalProcessingTime_; // 总处理时间（毫秒）

    // 调色板缓存（优化：仅提取一次）
    bool paletteInitialized_;                 // 调色板是否已初始化
    float thresholdGrayValue_;                // 高温阈值灰度值
    float percentileThreshold_;               // 百分位数阈值 (0.7=70%, 0.8=80%, 0.9=90%)
    static constexpr float HIGH_TEMP = 50.0f; // 高温赋值
    static constexpr float LOW_TEMP = 25.0f;  // 低温赋值
};