#include "TaskLocating.h"
#include <iostream>
#include <chrono>

/**
 * @brief 构造函数，初始化热成像检测任务
 * 注意：此类现在只负责热成像检测，不再负责定位上报
 * @param data 共享数据引用
 */
TaskLocating::TaskLocating(SharedData &data)
    : data_(data)
{
    std::cout << "[TaskLocating] 初始化热成像检测任务" << std::endl;
    // 初始化双设备的跟踪物体列表
    trackedObjects_.resize(2); // 支持2个设备（索引0和1）
}

/**
 * @brief 析构函数，确保线程安全退出
 */
TaskLocating::~TaskLocating()
{
    std::cout << "[TaskLocating] 开始析构..." << std::endl;
    stop(); // 停止线程
    std::cout << "[TaskLocating] 析构完成" << std::endl;
}

/**
 * @brief 启动热成像检测线程
 */
void TaskLocating::start()
{
    std::cout << "[TaskLocating] 启动热成像检测线程..." << std::endl;
    thread_ = std::thread(&TaskLocating::run, this); // 创建并启动线程
    std::cout << "[TaskLocating] 热成像检测线程启动成功" << std::endl;
}

/**
 * @brief 停止热成像检测线程
 */
void TaskLocating::stop()
{
    std::cout << "[TaskLocating] 停止热成像检测线程..." << std::endl;
    if (thread_.joinable())
    {                   // 如果线程可加入
        thread_.join(); // 等待线程结束
    }
    std::cout << "[TaskLocating] 热成像检测线程已停止" << std::endl;
}

/**
 * @brief 线程主函数，循环处理热成像数据
 *
 * 核心功能：
 * 1. 从SharedData中获取热成像温度矩阵
 * 2. 检测高温物体
 * 3. 设置检测标志位供上报线程使用
 * 4. 不再直接负责上报逻辑
 */
void TaskLocating::run()
{
    std::cout << "[TaskLocating] 热成像检测线程开始运行..." << std::endl;

    while (data_.isRunning)
    {
        cv::Mat thermalMatrix1; // 用于存储热成像矩阵1
        cv::Mat thermalMatrix2; // 用于存储热成像矩阵2

        // 加锁保护共享数据，避免多线程竞争
        {
            std::lock_guard<std::mutex> lock(data_.thermalmatrix_mutex_1);
            if (!data_.thermalMatrix_1.empty())
            {                                                 // 如果热成像矩阵1不为空
                data_.thermalMatrix_1.copyTo(thermalMatrix1); // 复制热成像矩阵1
            }
        }

        {
            std::lock_guard<std::mutex> lock2(data_.thermalmatrix_mutex_2);
            if (!data_.thermalMatrix_2.empty())
            {                                                 // 如果热成像矩阵2不为空
                data_.thermalMatrix_2.copyTo(thermalMatrix2); // 复制热成像矩阵2
            }
        }

        if (!thermalMatrix1.empty())
        {                                                 // 如果热成像矩阵1有效
            processThermalData(thermalMatrix1, data_, 0); // 处理设备1(一位端)热成像数据
        }

        if (!thermalMatrix2.empty())
        {                                                 // 如果热成像矩阵2有效
            processThermalData(thermalMatrix2, data_, 1); // 处理设备2(二位端)热成像数据
        }

        // 控制线程运行频率，避免占用过多CPU资源
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << "[TaskLocating] 热成像检测线程退出" << std::endl;
}

/**
 * @brief 处理热成像数据，检测高温物体并设置检测标志位
 *
 * 核心功能：
 * 1. 温度阈值分割，检测高温区域
 * 2. 轮廓检测，识别高温物体
 * 3. 去重逻辑，避免重复检测
 * 4. 根据设备索引设置对应的检测标志位供统一上报线程使用
 *
 * @param thermalMatrix 热成像温度矩阵
 * @param data 共享数据引用
 * @param deviceIndex 设备索引 (0=设备1/一位端, 1=设备2/二位端)
 */
void TaskLocating::processThermalData(const cv::Mat &thermalMatrix, SharedData &data, int deviceIndex)
{
    // 1. 温度阈值分割，生成二值掩码
    cv::Mat thresholdMask;
    if (thermalMatrix.empty())
    {
        std::cerr << "[TaskLocating] 热成像矩阵为空，无法处理数据" << std::endl;
        return;
    }
    // 获取报警阈值
    {
        std::lock_guard<std::mutex> lock(data_.alarmThresholdMutex);                                // 加锁保护报警阈值
        cv::threshold(thermalMatrix, thresholdMask, data.g_alarmThreshold, 255, cv::THRESH_BINARY); // 二值化处理
        // float g_alarmThreshold = data_.g_alarmThreshold; // 获取报警阈值
    }
    thresholdMask.convertTo(thresholdMask, CV_8UC1); // 转换为单通道8位图像

    // 2. 查找高温区域轮廓
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(thresholdMask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    // 存储当前帧检测到的高温物体中心点
    std::vector<cv::Point2f> currentObjects;
    for (const auto &contour : contours)
    {
        if (cv::contourArea(contour) > 100)
        {
            // 忽略面积小于100的噪声,计算高温物体的中心点
            cv::Moments m = cv::moments(contour);
            cv::Point2f center(static_cast<float>(m.m10 / m.m00), static_cast<float>(m.m01 / m.m00));
            currentObjects.push_back(center); // 添加到当前物体列表
        }
    }

    // 3. 去重逻辑，避免重复计数（使用设备特定的跟踪列表）
    {
        std::lock_guard<std::mutex> lock(trackedObjectsMutex_); // 加锁保护已跟踪物体列表

        // 验证设备索引有效性
        if (deviceIndex < 0 || deviceIndex >= static_cast<int>(trackedObjects_.size()))
        {
            std::cerr << "[TaskLocating] 错误：无效的设备索引 " << deviceIndex << std::endl;
            return;
        }

        auto &deviceTrackedObjects = trackedObjects_[deviceIndex]; // 获取当前设备的跟踪列表

        for (const auto &obj : currentObjects)
        {
            bool isNewObject = true; // 标记是否为新物体
            for (const auto &tracked : deviceTrackedObjects)
            {
                if (cv::norm(obj - tracked) < 50.0f)
                { // 如果距离小于50像素，认为是同一物体
                    isNewObject = false;
                    break;
                }
            }
            if (isNewObject)
            {                                        // 如果是新物体
                deviceTrackedObjects.push_back(obj); // 添加到当前设备的跟踪物体列表

                // 根据设备索引设置对应的热成像检测标志位（线程安全）
                if (deviceIndex == 0)
                {
                    data_.camera1_thermal_detected = true; // 设备1(一位端)热成像检测
                    std::cout << "[TaskLocating] 设备1(一位端)检测到新高温物体，设置检测标志位..." << std::endl;
                }
                else if (deviceIndex == 1)
                {
                    data_.camera2_thermal_detected = true; // 设备2(二位端)热成像检测
                    std::cout << "[TaskLocating] 设备2(二位端)检测到新高温物体，设置检测标志位..." << std::endl;
                }
            }
        }

        // 清理当前设备已离开视野的物体，避免列表过长
        if (deviceTrackedObjects.size() > 100)
        { // 假设最多跟踪100个物体
            deviceTrackedObjects.erase(deviceTrackedObjects.begin(), deviceTrackedObjects.begin() + 10);
        }
    }
}
