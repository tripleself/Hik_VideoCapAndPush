#pragma once
#include <thread>
#include <memory>
#include <chrono>
#include <opencv2/opencv.hpp>
#include "SharedData.h"
#include "ObjectTrackingConfig.h"

// 避免在头文件中包含Windows相关头文件，使用基本类型
// Windows相关的包含将在.cpp文件中处理

// 前向声明yolo_track库的类，避免头文件依赖
class YoloDetector;
class TrackerModule;
class CountingLineModule;
struct Detection;
struct TrackResult;

/**
 * @brief 目标追踪任务类
 * 负责处理可见光视频流的目标检测、追踪和计数功能
 * 集成YOLO检测器、ByteTrack追踪器和虚拟检测线计数模块
 *
 * 数据流向：
 * visible_video_Frame_1/2 → YOLO检测 → ByteTrack追踪 → 计数统计 → processedVisibleframe_1/2
 */
class TaskObjectTracking
{
public:
    /**
     * @brief 构造函数，初始化目标追踪任务
     * @param data 共享数据对象引用，用于线程间数据交换
     * @param config 追踪配置参数，包含所有模块的配置信息
     */
    explicit TaskObjectTracking(SharedData &data, const ObjectTrackingConfig &config = ObjectTrackingConfig{});

    /**
     * @brief 析构函数，确保线程安全退出和资源释放
     */
    ~TaskObjectTracking();

    /**
     * @brief 启动目标追踪线程
     * 初始化YOLO检测器、追踪器和计数器，然后启动处理线程
     */
    void start();

    /**
     * @brief 停止目标追踪线程
     * 设置停止标志，等待线程安全退出，释放所有资源
     */
    void stop();

private:
    /**
     * @brief 线程主函数，循环处理两路可见光视频流
     * 从SharedData读取原始视频帧，进行检测追踪处理，输出到处理后的视频帧
     */
    void run();

    /**
     * @brief 处理单帧视频的核心函数
     * @param inputFrame 输入的原始视频帧 (BGR格式)
     * @param outputFrame 输出的处理后视频帧 (BGR格式)
     * @param cameraId 摄像头ID (1或2，用于区分不同的计数器)
     * @return 当前帧检测到的目标数量
     */
    int processFrame(const cv::Mat &inputFrame, cv::Mat &outputFrame, int cameraId);

    /**
     * @brief 初始化所有追踪模块 (YOLO检测器、追踪器、计数器)
     * @return true 初始化成功，false 初始化失败
     */
    bool initializeModules();

    /**
     * @brief 显示性能统计信息到视频帧上
     * @param frame 要显示统计信息的视频帧
     * @param detectTime 检测耗时 (毫秒)
     * @param trackTime 追踪耗时 (毫秒)
     * @param objectCount 检测到的目标数量
     * @param totalCount 累计计数
     */
    void drawPerformanceStats(cv::Mat &frame, double detectTime, double trackTime,
                              int objectCount, int totalCount);

private:
    // ========== 核心数据成员 ==========
    SharedData &data_;            // 共享数据对象引用
    ObjectTrackingConfig config_; // 追踪配置参数
    std::thread thread_;          // 处理线程对象

    // ========== YOLO追踪模块 (智能指针管理，延迟初始化) ==========
    std::unique_ptr<YoloDetector> detector_;       // YOLO目标检测器
    std::unique_ptr<TrackerModule> tracker1_;      // 一位端ByteTrack追踪器
    std::unique_ptr<TrackerModule> tracker2_;      // 二位端ByteTrack追踪器
    std::unique_ptr<CountingLineModule> counter1_; // 一位端虚拟检测线计数器
    std::unique_ptr<CountingLineModule> counter2_; // 二位端虚拟检测线计数器

    // ========== 性能统计变量 ==========
    std::chrono::steady_clock::time_point lastStatsTime_; // 上次统计时间点
    int frameCount_;                                      // 处理帧数计数器
    double totalDetectTime_;                              // 累计检测时间
    double totalTrackTime_;                               // 累计追踪时间

    // ========== 线程控制标志 ==========
    std::atomic<bool> initialized_; // 模块初始化完成标志
};