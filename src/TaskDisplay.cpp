#include "TaskDisplay.h"
#include <opencv2/opencv.hpp>
#include <iostream>
#include <cstdlib> // 用于rand()函数
#include <thread>  // 用于std::thread
#include <mutex>   // 用于std::mutex
#include <chrono>  // 用于std::chrono

// 构造函数，初始化显示控制标志和窗口名称
TaskDisplay::TaskDisplay(SharedData &data, bool enableDisplay) : data_(data), enableDisplay_(enableDisplay), windowInitialized_(false), windowName_("Thermal Imaging Analysis")
{
}

// 析构函数，调用 stop 确保线程安全退出
TaskDisplay::~TaskDisplay()
{
    stop();
}

// 启动显示线程
void TaskDisplay::start()
{
    thread_ = std::thread(&TaskDisplay::run, this);
}

// 停止显示线程
void TaskDisplay::stop()
{
    data_.isRunning = false;
    if (thread_.joinable())
    {
        thread_.join();
    }
    cleanupDisplay();
}

// 设置是否启用窗口显示
void TaskDisplay::setDisplayEnabled(bool enabled)
{
    enableDisplay_ = enabled;
    if (!enabled)
    {
        cleanupDisplay();
    }
}

// 获取显示状态
bool TaskDisplay::isDisplayEnabled() const
{
    return enableDisplay_;
}

// 线程主函数
void TaskDisplay::run()
{
    // 如果启用显示，初始化窗口
    if (enableDisplay_)
    {
        initializeDisplay();
    }

    while (data_.isRunning)
    {
        // 处理视频帧和温度数据（核心逻辑）
        processVideoFrames();

        // 如果启用显示，更新显示
        if (enableDisplay_ && windowInitialized_)
        {
            cv::Mat displayFrame;
            std::string deviceInfo = "";

            // 优先显示设备1的数据，如果没有则显示设备2的数据
            {
                std::lock_guard<std::mutex> lock(data_.processed_thermal_mutex_1);
                if (!data_.processed_thermal_frame_1.empty())
                {
                    data_.processed_thermal_frame_1.copyTo(displayFrame);
                    deviceInfo = " - 设备1(一位端)热成像";
                }
            }

            // 如果设备1没有数据，尝试显示设备2的数据
            if (displayFrame.empty())
            {
                std::lock_guard<std::mutex> lock(data_.processed_thermal_mutex_2);
                if (!data_.processed_thermal_frame_2.empty())
                {
                    data_.processed_thermal_frame_2.copyTo(displayFrame);
                    deviceInfo = " - 设备2(二位端)热成像";
                }
            }

            if (!displayFrame.empty())
            {
                // 更新窗口标题显示当前设备信息
                std::string windowTitle = windowName_ + deviceInfo;
                cv::setWindowTitle(windowName_, windowTitle);
                updateDisplay(displayFrame);
            }

            // 检查窗口状态和按键
            double visible = cv::getWindowProperty(windowName_, cv::WND_PROP_VISIBLE);
            if (visible <= 0)
            {
                data_.isRunning = false;
                break;
            }

            if (cv::waitKey(30) == 27) // ESC键退出
            {
                data_.isRunning = false;
                break;
            }
        }
        else
        {
            // 无显示模式下的短暂延时，避免CPU占用过高
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
        }
    }
}

// 处理视频帧和温度数据（核心处理逻辑）
void TaskDisplay::processVideoFrames()
{
    cv::Mat displayFrame, thermalMatrix;
    cv::Mat displayFrame2, thermalMatrix2;

    // 处理第一路热成像视频
    
    std::lock_guard<std::mutex> lock1(data_.thermal_mutex_1);
    std::lock_guard<std::mutex> lock2(data_.thermalmatrix_mutex_1);

    // 测试模式，无温度数据
    if (!data_.thermal_video_frame_1.empty() && data_.thermalMatrix_1.empty())
        {
            std::cout << "process fake TemperatureData" << std::endl;
            data_.thermal_video_frame_1.copyTo(displayFrame);

            // 创建虚假温度矩阵 (640x512 对应热成像分辨率)
            data_.thermalMatrix_1 = cv::Mat::zeros(512, 640, CV_32F);
            cv::randu(data_.thermalMatrix_1, cv::Scalar(20.0f), cv::Scalar(35.0f));

            // 添加模拟的高温区域
            std::vector<cv::Point> hotSpots = {
                cv::Point(150, 120), cv::Point(350, 250), cv::Point(500, 380)};

            for (size_t i = 0; i < hotSpots.size(); ++i)
            {
                const auto &spot = hotSpots[i];
                float hotTemp = 45.0f + (i * 5.0f) + (rand() % 10);
                cv::circle(data_.thermalMatrix_1, spot, 25 + (i * 5), cv::Scalar(hotTemp), -1);
                cv::circle(data_.thermalMatrix_1, spot, 35 + (i * 5), cv::Scalar(hotTemp - 5.0f), 3);
            }

            // 添加噪声
            cv::Mat noise(512, 640, CV_32F);
            cv::randu(noise, cv::Scalar(-2.0f), cv::Scalar(2.0f));
            data_.thermalMatrix_1 += noise;
            data_.thermalMatrix_1.copyTo(thermalMatrix);
        }

    // 真实模式，有温度数据
    if (!data_.thermal_video_frame_1.empty() && !data_.thermalMatrix_1.empty())
    {
        data_.thermal_video_frame_1.copyTo(displayFrame);
        data_.thermalMatrix_1.copyTo(thermalMatrix);
    }


    // 处理第二路热成像视频
    
    std::lock_guard<std::mutex> lock3(data_.thermal_mutex_2);
    std::lock_guard<std::mutex> lock4(data_.thermalmatrix_mutex_2);

    // 测试模式，生成第二路虚假温度数据
    if (!data_.thermal_video_frame_2.empty() && data_.thermalMatrix_2.empty())
        {
            std::cout << "process fake TemperatureData2" << std::endl;
            data_.thermalMatrix_2 = cv::Mat::zeros(512, 640, CV_32F);
            cv::randu(data_.thermalMatrix_2, cv::Scalar(22.0f), cv::Scalar(37.0f));

            std::vector<cv::Point> hotSpots2 = {
                cv::Point(200, 180), cv::Point(400, 300), cv::Point(100, 420)};

            for (size_t i = 0; i < hotSpots2.size(); ++i)
            {
                const auto &spot = hotSpots2[i];
                float hotTemp = 48.0f + (i * 6.0f) + (rand() % 8);
                cv::circle(data_.thermalMatrix_2, spot, 20 + (i * 4), cv::Scalar(hotTemp), -1);
                cv::circle(data_.thermalMatrix_2, spot, 30 + (i * 4), cv::Scalar(hotTemp - 4.0f), 2);
            }

            cv::Mat noise2(512, 640, CV_32F);
            cv::randu(noise2, cv::Scalar(-1.5f), cv::Scalar(1.5f));
            data_.thermalMatrix_2 += noise2;
        }
        
    // 真实模式，有温度数据
    if (!data_.thermal_video_frame_2.empty() && !data_.thermalMatrix_2.empty())
    {
        data_.thermal_video_frame_2.copyTo(displayFrame2);
        data_.thermalMatrix_2.copyTo(thermalMatrix2);
    }
    

    // 处理第一路显示和RTSP输出
    if (!displayFrame.empty() && !thermalMatrix.empty())
    {
        // std::cout << "process True TemperatureData1" << std::endl;
        processTemperatureData(thermalMatrix, displayFrame);
        // RTSP 输出 - 复制处理后的第一路热成像帧
        std::lock_guard<std::mutex> lock5(data_.processed_thermal_mutex_1);
        displayFrame.copyTo(data_.processed_thermal_frame_1);
    }

    // 处理第二路显示和RTSP输出

    if (!displayFrame2.empty() && !thermalMatrix2.empty())
    {
        // std::cout << "process True TemperatureData2" << std::endl;  
        processTemperatureData(thermalMatrix2, displayFrame2);
        // RTSP 输出 - 复制处理后的第二路热成像帧
        std::lock_guard<std::mutex> lock6(data_.processed_thermal_mutex_2);
        displayFrame2.copyTo(data_.processed_thermal_frame_2);
    }
}

// 初始化显示窗口
void TaskDisplay::initializeDisplay()
{
    if (!windowInitialized_)
    {
        cv::namedWindow(windowName_, cv::WINDOW_NORMAL | cv::WINDOW_GUI_EXPANDED);
        cv::resizeWindow(windowName_, 800, 600);
        windowInitialized_ = true;
    }
}

// 更新窗口显示
void TaskDisplay::updateDisplay(const cv::Mat &displayFrame)
{
    if (windowInitialized_ && !displayFrame.empty())
    {
        cv::imshow(windowName_, displayFrame);
    }
}

// 清理显示窗口
void TaskDisplay::cleanupDisplay()
{
    if (windowInitialized_)
    {
        cv::destroyWindow(windowName_);
        windowInitialized_ = false;
    }
}

// 处理温度数据，在视频帧上绘制高温区域
void TaskDisplay::processTemperatureData(const cv::Mat &tempMatrix, cv::Mat &frame)
{
    float scaleX = 1280.0f / 640.0f;
    float scaleY = 720.0f / 512.0f;

    if (tempMatrix.empty() || frame.empty())
        return;

    float alarmThreshold;
    {
        std::lock_guard<std::mutex> lock3(data_.alarmThresholdMutex);
        alarmThreshold = data_.g_alarmThreshold;
    }

    // 温度阈值分割
    cv::Mat thresholdMask;
    cv::threshold(tempMatrix, thresholdMask, alarmThreshold, 255, cv::THRESH_BINARY);
    thresholdMask.convertTo(thresholdMask, CV_8UC1);

    // 形态学滤波
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(5, 5));
    cv::morphologyEx(thresholdMask, thresholdMask, cv::MORPH_OPEN, kernel);

    // 查找高温区域轮廓
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(thresholdMask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    // 绘制最小外接矩形
    for (const auto &contour : contours)
    {
        if (cv::contourArea(contour) > 100)
        {
            std::vector<cv::Point> scaledContour;
            for (const auto &pt : contour)
            {
                scaledContour.emplace_back(cvRound(pt.x * scaleX), cvRound(pt.y * scaleY));
            }
            cv::RotatedRect rect = cv::minAreaRect(scaledContour);
            cv::Point2f vertices[4];
            rect.points(vertices);
            for (int i = 0; i < 4; i++)
            {
                cv::line(frame, vertices[i], vertices[(i + 1) % 4], cv::Scalar(0, 255, 255), 2);
            }
        }
    }
}