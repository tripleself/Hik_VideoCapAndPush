#pragma once
#include <thread>
#include <mutex>
#include <vector>
#include "SharedData.h"
/**
 * @brief 热成像检测任务类
 *
 * 职责：
 * 1. 从SharedData中获取热成像温度矩阵
 * 2. 检测高温物体（温度阈值分割、轮廓检测）
 * 3. 设置检测标志位供统一上报线程使用
 * 4. 不再直接负责定位上报逻辑
 */
class TaskLocating
{
public:
    /**
     * @brief 构造函数
     * @param data 共享数据引用
     */
    explicit TaskLocating(SharedData &data);

    /**
     * @brief 析构函数，确保线程安全停止
     */
    ~TaskLocating();

    /**
     * @brief 启动热成像检测线程
     */
    void start();

    /**
     * @brief 停止热成像检测线程
     */
    void stop();

private:
    /**
     * @brief 线程主函数，循环处理热成像数据
     */
    void run();

    /**
     * @brief 处理热成像数据，检测高温物体并设置检测标志位
     * @param thermalMatrix 热成像温度矩阵
     * @param data 共享数据引用
     * @param deviceIndex 设备索引 (0=设备1/一位端, 1=设备2/二位端)
     */
    void processThermalData(const cv::Mat &thermalMatrix, SharedData &data, int deviceIndex);

    // 核心成员变量
    SharedData &data_;                                     // 共享数据引用
    std::thread thread_;                                   // 热成像检测线程
    std::mutex trackedObjectsMutex_;                       // 跟踪物体的互斥锁
    std::vector<std::vector<cv::Point2f>> trackedObjects_; // 每个设备的跟踪物体列表[设备索引][物体列表]
};