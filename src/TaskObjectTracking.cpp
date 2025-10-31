#include <thread>
#include <mutex>
#include <chrono>
#include <memory>
#include "TaskObjectTracking.h"
#include <iostream>
#include <filesystem>
#include <iomanip>

// 包含yolo_track库的头文件
#include "config_manager.h"
#include "infer.h"
#include "tracker.h"
#include "counting_line.h"

// 构造函数，初始化目标追踪任务
TaskObjectTracking::TaskObjectTracking(SharedData &data, const ObjectTrackingConfig &config)
    : data_(data),
      config_(config),
      frameCount_(0),
      totalDetectTime_(0.0),
      totalTrackTime_(0.0),
      initialized_(false)
{
    // 验证配置参数有效性
    if (!config_.isValid())
    {
        std::cerr << "[TaskObjectTracking] 目标追踪配置参数无效！" << std::endl;
        return;
    }

    std::cout << "[TaskObjectTracking] 目标追踪任务初始化完成" << std::endl;
}

TaskObjectTracking::~TaskObjectTracking()
{
    stop();
}

void TaskObjectTracking::start()
{
    // 检查ConfigManager和模型文件
    auto configMgr = config_.getConfigManager();
    if (!configMgr)
    {
        std::cerr << "[TaskObjectTracking] ConfigManager未初始化" << std::endl;
        return;
    }

    if (!std::filesystem::exists(configMgr->getEnginePath()))
    {
        std::cerr << "[TaskObjectTracking] 错误：找不到模型文件 " << configMgr->getEnginePath() << std::endl;
        return;
    }

    // 启动处理线程
    thread_ = std::thread(&TaskObjectTracking::run, this);
    std::cout << "[TaskObjectTracking] 目标追踪线程已启动" << std::endl;
}

void TaskObjectTracking::stop()
{
    data_.isRunning = false;
    std::cout << "[TaskObjectTracking] 目标追踪线程已退出" << std::endl;

    if (thread_.joinable())
    {
        thread_.join();
    }

    // 释放资源
    detector_.reset();
    tracker1_.reset();
    tracker2_.reset();
    counter1_.reset();
    counter2_.reset();
}

/**
 * @brief 初始化所有追踪模块（使用ConfigManager统一管理参数）
 */
bool TaskObjectTracking::initializeModules()
{
    try
    {
        // 获取ConfigManager实例
        auto configMgr = config_.getConfigManager();
        if (!configMgr)
        {
            std::cerr << "[TaskObjectTracking] ConfigManager未初始化" << std::endl;
            return false;
        }

        // 1. 初始化YOLO检测器（使用ConfigManager）
        detector_ = std::make_unique<YoloDetector>(*configMgr);
        std::cout << "[TaskObjectTracking] YOLO检测器初始化完成" << std::endl;

        // 2. 初始化两个ByteTrack追踪器（使用ConfigManager）
        tracker1_ = std::make_unique<TrackerModule>(*configMgr);
        tracker2_ = std::make_unique<TrackerModule>(*configMgr);
        std::cout << "[TaskObjectTracking] ByteTrack追踪器初始化完成" << std::endl;

        // 3. 初始化计数模块（如果启用）
        if (configMgr->isCountingEnabled())
        {
            // 一位端计数器
            counter1_ = std::make_unique<CountingLineModule>(
                config_.videoWidth,
                config_.videoHeight,
                config_.processingFps,
                *configMgr,
                "camera_1");
            counter1_->startCounting();

            // 二位端计数器
            counter2_ = std::make_unique<CountingLineModule>(
                config_.videoWidth,
                config_.videoHeight,
                config_.processingFps,
                *configMgr,
                "camera_2");
            counter2_->startCounting();

            std::cout << "[TaskObjectTracking] 虚拟检测线计数模块初始化完成" << std::endl;
        }

        std::cout << "[TaskObjectTracking] 目标追踪模块初始化完成" << std::endl;
        initialized_ = true;
        return true;
    }
    catch (const std::exception &e)
    {
        std::cerr << "[TaskObjectTracking] 模块初始化失败: " << e.what() << std::endl;
        return false;
    }
}

void TaskObjectTracking::run()
{
    // 初始化所有模块
    if (!initializeModules())
    {
        std::cerr << "[TaskObjectTracking] 模块初始化失败，退出追踪线程" << std::endl;
        return;
    }

    // 创建显示窗口 (默认启用)
    if (config_.enableDisplay)
    {
        cv::namedWindow(config_.windowName, cv::WINDOW_NORMAL);
        cv::resizeWindow(config_.windowName, config_.windowWidth, config_.windowHeight);
    }

    // 性能统计初始化
    lastStatsTime_ = std::chrono::steady_clock::now();

    cv::Mat visibleFrame1, visibleFrame2;     // 输入帧缓冲区
    cv::Mat processedFrame1, processedFrame2; // 输出帧缓冲区

    while (data_.isRunning && initialized_)
    {

        // ========== 处理一位端可见光视频流 ==========
        {
            std::lock_guard<std::mutex> lock(data_.visible_mutex_1);
            if (!data_.visible_video_frame_1.empty())
            {
                data_.visible_video_frame_1.copyTo(visibleFrame1);
            }
        }

        if (!visibleFrame1.empty())
        {
            int objectCount1 = processFrame(visibleFrame1, processedFrame1, 1);

            // 将处理后的帧写入共享数据
            {
                std::lock_guard<std::mutex> lock(data_.processed_visible_mutex_1);
                processedFrame1.copyTo(data_.processed_visible_frame_1);
            }

            // 更新检测目标数量
            data_.detectedObjectCount_1 = objectCount1;
        }
        // ========== 处理二位端可见光视频流 ==========
        {
            std::lock_guard<std::mutex> lock(data_.visible_mutex_2);
            if (!data_.visible_video_frame_2.empty())
            {
                data_.visible_video_frame_2.copyTo(visibleFrame2);
            }
        }
        if (!visibleFrame2.empty())
        {
            int objectCount2 = processFrame(visibleFrame2, processedFrame2, 2);

            // 将处理后的帧写入共享数据
            {
                std::lock_guard<std::mutex> lock(data_.processed_visible_mutex_2);
                processedFrame2.copyTo(data_.processed_visible_frame_2);
            }

            // 更新检测目标数量
            data_.detectedObjectCount_2 = objectCount2;
        }

        // ========== 显示处理结果 (如果启用) ==========
        if (config_.enableDisplay)
        {
            cv::Mat displayFrame;
            std::string windowTitle = config_.windowName;

            // 优先显示设备1的数据，如果没有则显示设备2的数据
            if (!processedFrame1.empty())
            {
                displayFrame = processedFrame1.clone();
                windowTitle += " - 设备1(一位端)";
            }
            else if (!processedFrame2.empty())
            {
                displayFrame = processedFrame2.clone();
                windowTitle += " - 设备2(二位端)";
            }

            if (!displayFrame.empty())
            {
                // 更新窗口标题以显示当前显示的是哪个设备
                cv::setWindowTitle(config_.windowName, windowTitle);
                cv::imshow(config_.windowName, displayFrame);

                // 检查窗口是否被关闭
                if (cv::waitKey(1) == 27)
                { // ESC键退出
                    data_.isRunning = false;
                    break;
                }
            }
        }

        // 控制处理频率，避免过度占用CPU
        std::this_thread::sleep_for(std::chrono::milliseconds(config_.threadSleepMs));
    }

    // 完成计数统计
    auto configMgr = config_.getConfigManager();
    if (configMgr && configMgr->isCountingEnabled())
    {
        if (counter1_)
            counter1_->finishCounting(frameCount_);
        if (counter2_)
            counter2_->finishCounting(frameCount_);
    }

    // 关闭显示窗口
    if (config_.enableDisplay)
    {
        cv::destroyWindow(config_.windowName);
    }
}

// 处理单帧视频
int TaskObjectTracking::processFrame(const cv::Mat &inputFrame, cv::Mat &outputFrame, int cameraId)
{
    if (inputFrame.empty())
        return 0;

    // 复制输入帧到输出帧
    inputFrame.copyTo(outputFrame);

    auto frameStartTime = std::chrono::steady_clock::now();

    // ========== 1. YOLO目标检测 ==========
    auto detectStart = std::chrono::steady_clock::now();
    std::vector<Detection> detections = detector_->inference(outputFrame);
    auto detectEnd = std::chrono::steady_clock::now();

    double detectTime = std::chrono::duration<double, std::milli>(detectEnd - detectStart).count();
    totalDetectTime_ += detectTime;

    // ========== 2. ByteTrack目标追踪 ==========
    auto trackStart = std::chrono::steady_clock::now();
    std::vector<TrackResult> tracks;

    if (cameraId == 1 && tracker1_)
    {
        tracks = tracker1_->update(detections);
    }
    else if (cameraId == 2 && tracker2_)
    {
        tracks = tracker2_->update(detections);
    }

    auto trackEnd = std::chrono::steady_clock::now();
    double trackTime = std::chrono::duration<double, std::milli>(trackEnd - trackStart).count();
    totalTrackTime_ += trackTime;

    // ========== 3. 虚拟检测线计数 ==========
    int totalCount = 0;
    auto configMgr = config_.getConfigManager();
    if (configMgr && configMgr->isCountingEnabled())
    {
        // 计算当前帧时间戳
        double currentFrameTime = frameCount_ * (1000.0 / config_.processingFps);
        double realProcessingTime = detectTime + trackTime;

        if (cameraId == 1 && counter1_)
        {
            int newCrossings = counter1_->updateCounting(tracks, currentFrameTime, realProcessingTime);
            counter1_->drawDetectionLine(outputFrame);
            totalCount = counter1_->getTotalCount();

            // 检测到新目标时设置检测标志位
            if (newCrossings > 0)
            {
                data_.camera1_visible_detected = true;
            }
        }
        else if (cameraId == 2 && counter2_)
        {
            int newCrossings = counter2_->updateCounting(tracks, currentFrameTime, realProcessingTime);
            counter2_->drawDetectionLine(outputFrame);
            totalCount = counter2_->getTotalCount();

            // 检测到新目标时设置检测标志位
            if (newCrossings > 0)
            {
                data_.camera2_visible_detected = true;
            }
        }
    }

    // ========== 4. 绘制追踪结果 ==========
    TrackerModule::drawTrackResults(outputFrame, tracks);

    // ========== 5. 显示性能统计 (如果启用) ==========
    if (config_.enablePerformanceStats)
    {
        drawPerformanceStats(outputFrame, detectTime, trackTime, tracks.size(), totalCount);
    }

    frameCount_++;
    return static_cast<int>(tracks.size());
}

// 显示性能统计信息到视频帧上
void TaskObjectTracking::drawPerformanceStats(cv::Mat &frame, double detectTime, double trackTime,
                                              int objectCount, int totalCount)
{
    // // 性能信息文本
    // std::vector<std::string> statsText = {
    //     "Frame: " + std::to_string(frameCount_),
    //     "Detect: " + std::to_string(static_cast<int>(detectTime)) + "ms",
    //     "Track: " + std::to_string(static_cast<int>(trackTime)) + "ms",
    //     "Objects: " + std::to_string(objectCount),
    //     "Total Count: " + std::to_string(totalCount)};

    // // 绘制半透明背景
    // cv::Rect statsRect(10, 10, 200, static_cast<int>(statsText.size()) * 25 + 10);
    // cv::rectangle(frame, statsRect, cv::Scalar(0, 0, 0), -1);
    // cv::rectangle(frame, statsRect, cv::Scalar(255, 255, 255), 1);

    // // 绘制统计文本
    // for (size_t i = 0; i < statsText.size(); ++i)
    // {
    //     cv::putText(frame, statsText[i],
    //                 cv::Point(15, 30 + static_cast<int>(i) * 25),
    //                 cv::FONT_HERSHEY_SIMPLEX, 0.5,
    //                 cv::Scalar(0, 255, 0), 1);
    // }

    // 定期输出平均性能统计
    auto currentTime = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(currentTime - lastStatsTime_);

    if (duration.count() >= 5 && frameCount_ > 0)
    { // 每5秒输出一次
        double avgDetectTime = totalDetectTime_ / frameCount_;
        double avgTrackTime = totalTrackTime_ / frameCount_;
        double avgFps = frameCount_ / duration.count();

        std::cout << "=== [TaskObjectTracking] 性能统计 (过去5秒) ===" << std::endl;
        std::cout << "平均检测时间: " << std::fixed << std::setprecision(1) << avgDetectTime << " ms/frame" << std::endl;
        std::cout << "平均追踪时间: " << std::fixed << std::setprecision(1) << avgTrackTime << " ms/frame" << std::endl;
        std::cout << "平均处理帧率: " << std::fixed << std::setprecision(1) << avgFps << " fps" << std::endl;
        std::cout << "当前追踪目标: " << objectCount << " 个" << std::endl;
        std::cout << "累计计数: " << totalCount << " 个" << std::endl;

        // 重置统计
        lastStatsTime_ = currentTime;
        frameCount_ = 0;
        totalDetectTime_ = 0.0;
        totalTrackTime_ = 0.0;
    }
}

// ========== 定位上报功能实现 ==========
