#pragma once
#include <opencv2/opencv.hpp>
#include <mutex>
#include <atomic>
#include <string>
#include <filesystem>

// 海康威视SDK头文件，提供BYTE、DWORD等类型定义
#include "HCNetSDK.h"

// ========== 实时温度数据结构 ==========
/**
 * @brief 实时温度数据结构
 * 存储从海康热成像相机获取的实时温度信息
 */
struct RealTimeTemperatureData
{
    float highestTemperature = 0.0f; // 最高温度 (°C)
    float lowestTemperature = 0.0f;  // 最低温度 (°C)
    float centerTemperature = 0.0f;  // 中心点温度 (°C)
    bool isValid = false;            // 数据有效性标志
    std::string ruleName = "";       // 测温规则名称
    BYTE ruleID = 0;                 // 规则ID号
    DWORD timestamp = 0;             // 原始时间戳
    DWORD channelNo = 0;             // 通道号

    // 解析后的时间信息
    std::string relativeTimeStr = ""; // 相对时标字符串 (带时区，如东八区时间)
    std::string absoluteTimeStr = ""; // 绝对时标字符串 (UTC时间)

    // 重置数据
    void reset()
    {
        highestTemperature = 0.0f;
        lowestTemperature = 0.0f;
        centerTemperature = 0.0f;
        isValid = false;
        ruleName = "";
        ruleID = 0;
        timestamp = 0;
        channelNo = 0;
        relativeTimeStr = "";
        absoluteTimeStr = "";
    }
};

// ========== Video Save Configuration Structure (Hikvision SDK) ==========
/**
 * @brief Video save configuration structure using Hikvision SDK
 * Stores SDK-based video save configuration parameters
 */
struct VideoSaveConfig
{
    bool enableVideoSave = false;                    // Whether to enable video save function
    std::string videoSavePath = "D:/RailwayVideos/"; // Video save directory path
    int maxFileSizeMB = 1024;                        // Max file size in MB (SDK auto-split threshold)
    int maxStorageGB = 600;                          // Max total storage in GB
    int cleanupSizeGB = 40;                          // Size to cleanup when exceeded in GB

    // Reset to default values
    void reset()
    {
        enableVideoSave = false;
        videoSavePath = "D:/RailwayVideos/";
        maxFileSizeMB = 1024;
        maxStorageGB = 600;
        cleanupSizeGB = 40;
    }
};

// ========== Thermal Processing Configuration Structure ==========
/**
 * @brief Thermal processing configuration structure
 * Stores thermal processing related configuration parameters
 */
struct ThermalProcessingConfig
{
    bool enableThermalProcessing = true;    // Whether to enable thermal processing
    float environmentTempThreshold = 50.0f; // Minimum environment temperature to start processing

    // Reset to default values
    void reset()
    {
        enableThermalProcessing = true;
        environmentTempThreshold = 50.0f;
    }
};

/**
 * @brief 共享数据结构（简化版）
 * 回退到简单的cv::Mat + std::mutex组合
 * 移除复杂的双缓冲区机制，确保数据流向清晰可控
 */
struct SharedData
{
    // ========== 视频帧数据（简化版）==========
    // 一位端视频数据
    cv::Mat thermal_video_frame_1;     // 一位端热成像视频帧
    cv::Mat visible_video_frame_1;     // 一位端可见光视频帧
    cv::Mat processed_thermal_frame_1; // 一位端处理后热成像帧
    cv::Mat processed_visible_frame_1; // 一位端处理后可见光帧

    // 二位端视频数据
    cv::Mat thermal_video_frame_2;     // 二位端热成像视频帧
    cv::Mat visible_video_frame_2;     // 二位端可见光视频帧
    cv::Mat processed_thermal_frame_2; // 二位端处理后热成像帧
    cv::Mat processed_visible_frame_2; // 二位端处理后可见光帧

    // ========== 同步锁（简化版）==========
    // 一位端同步锁
    std::mutex thermal_mutex_1;           // 一位端热成像数据锁
    std::mutex visible_mutex_1;           // 一位端可见光数据锁
    std::mutex processed_thermal_mutex_1; // 一位端处理后热成像锁
    std::mutex processed_visible_mutex_1; // 一位端处理后可见光锁

    // 二位端同步锁
    std::mutex thermal_mutex_2;           // 二位端热成像数据锁
    std::mutex visible_mutex_2;           // 二位端可见光数据锁
    std::mutex processed_thermal_mutex_2; // 二位端处理后热成像锁
    std::mutex processed_visible_mutex_2; // 二位端处理后可见光锁

    // ========== 温度数据（保持不变）==========
    cv::Mat thermalMatrix_1;          // 热成像温度矩阵（CV_32FC1单通道浮点）
    cv::Mat thermalMatrix_2;          // 热成像温度矩阵（CV_32FC1单通道浮点）
    std::mutex thermalmatrix_mutex_1; // 温度数据访问同步锁
    std::mutex thermalmatrix_mutex_2; // 温度数据访问同步锁

    // ========== 实时温度数据（新增）==========
    RealTimeTemperatureData realtimeTemp_1; // 一位端实时温度数据
    RealTimeTemperatureData realtimeTemp_2; // 二位端实时温度数据
    std::mutex realtimeTemp_mutex_1;        // 一位端实时温度数据同步锁
    std::mutex realtimeTemp_mutex_2;        // 二位端实时温度数据同步锁

    std::mutex alarmThresholdMutex; // 报警阈值访问同步锁

    // ========== 目标追踪相关数据（保持不变）==========
    std::atomic<int> detectedObjectCount_1{0}; // 一位端检测目标数量
    std::atomic<int> detectedObjectCount_2{0}; // 二位端检测目标数量
    std::mutex trackingMutex_1;                // 一位端追踪数据同步锁
    std::mutex trackingMutex_2;                // 二位端追踪数据同步锁

    // ========== 检测状态标志位（用于CAN上报）==========
    std::atomic<bool> camera1_visible_detected{false}; // 一位端可见光检测状态
    std::atomic<bool> camera1_thermal_detected{false}; // 一位端热成像检测状态
    std::atomic<bool> camera2_visible_detected{false}; // 二位端可见光检测状态
    std::atomic<bool> camera2_thermal_detected{false}; // 二位端热成像检测状态

    // ========== 系统控制标志位（保持不变）==========
    std::atomic_bool isRunning; // 线程控制标志位

    // ========== Video Save Configuration (New) ==========
    VideoSaveConfig videoSaveConfig; // Video save configuration
    std::mutex videoSaveConfigMutex; // Video save configuration sync lock

    // ========== Thermal Processing Configuration (New) ==========
    ThermalProcessingConfig thermalProcessingConfig; // Thermal processing configuration
    std::mutex thermalProcessingConfigMutex;         // Thermal processing configuration sync lock

    float g_alarmThreshold = 40.0f; // 报警阈值
};

// 视频流配置结构体（FFmpeg版本）
struct VideoStreamConfig
{
    std::string name;     // 视频流标识，如 "capT_1", "capV_1"
    std::string url;      // RTSP视频流地址
    std::string errorMsg; // 初始化失败时的错误信息

    // 构造函数
    VideoStreamConfig() = default;
    VideoStreamConfig(const std::string &n, const std::string &u, const std::string &e)
        : name(n), url(u), errorMsg(e) {}
};
