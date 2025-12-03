#pragma once
#include "SharedData.h"
#include "HCNetSDK.h"
#include <thread>
#include <vector>

/**
 * @brief 热成像数据捕获任务类
 * 负责从海康设备获取温度矩阵数据，支持多设备（1-2个摄像头）
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

private:
    /**
     * @brief 线程主函数，循环捕获热成像数据
     */
    void run();

    std::vector<LONG> userIDs_; // 海康设备用户ID列表
    SharedData &data_;          // 共享数据引用
    std::thread thread_;        // 热成像捕获线程
};