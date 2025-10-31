/**
 * @file integration_example.cpp
 * @brief 海康威视SDK集成示例代码
 *
 * 展示如何将HikCameraCapture类集成到您的应用程序中进行视频后处理
 */

#include "HikCameraCapture.h"
#include <opencv2/opencv.hpp>
#include <iostream>
#include <thread>
#include <chrono>

// 模拟的AI处理类
class AIProcessor
{
public:
    struct Detection
    {
        cv::Rect bbox;
        float confidence;
        std::string className;
    };

    std::vector<Detection> detectObjects(const cv::Mat &frame)
    {
        // 这里是您的AI模型推理代码
        // 例如：YOLO、SSD、或其他目标检测算法

        std::vector<Detection> detections;

        // 模拟检测结果
        if (!frame.empty())
        {
            Detection det;
            det.bbox = cv::Rect(100, 100, 200, 150);
            det.confidence = 0.85f;
            det.className = "person";
            detections.push_back(det);
        }

        return detections;
    }

    cv::Mat drawDetections(const cv::Mat &frame, const std::vector<Detection> &detections)
    {
        cv::Mat result = frame.clone();

        for (const auto &det : detections)
        {
            // 绘制边界框
            cv::rectangle(result, det.bbox, cv::Scalar(0, 255, 0), 2);

            // 绘制标签
            std::string label = det.className + " " + std::to_string(det.confidence);
            cv::putText(result, label,
                        cv::Point(det.bbox.x, det.bbox.y - 10),
                        cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(0, 255, 0), 2);
        }

        return result;
    }
};

// 应用程序主类
class VideoAnalysisApp
{
private:
    std::unique_ptr<HikCameraCapture> camera_;
    std::unique_ptr<AIProcessor> aiProcessor_;
    std::unique_ptr<VideoRecorder> recorder_;

    std::atomic<bool> running_{false};
    std::thread processingThread_;

    // 性能监控
    std::chrono::steady_clock::time_point lastMetricsTime_;
    int processedFrames_ = 0;

public:
    VideoAnalysisApp()
    {
        camera_ = std::make_unique<HikCameraCapture>();
        aiProcessor_ = std::make_unique<AIProcessor>();
        recorder_ = std::make_unique<VideoRecorder>();
        lastMetricsTime_ = std::chrono::steady_clock::now();
    }

    ~VideoAnalysisApp()
    {
        stop();
    }

    bool initialize(const std::string &cameraIP,
                    const std::string &username,
                    const std::string &password)
    {

        std::cout << "初始化摄像头连接..." << std::endl;

        // 设置回调函数
        camera_->setFrameCallback([this](const cv::Mat &frame, int channel)
                                  { onFrameReceived(frame, channel); });

        camera_->setErrorCallback([this](const std::string &error)
                                  { onError(error); });

        camera_->setStatusCallback([this](const PerformanceMetrics &metrics)
                                   { onStatusUpdate(metrics); });

        // 优化性能参数
        camera_->setPerformanceParams(1000, 5000, 512 * 1024, true);

        // 初始化摄像头
        if (!camera_->initialize(cameraIP, username, password, 8553))
        {
            std::cerr << "摄像头初始化失败！" << std::endl;
            return false;
        }

        std::cout << "摄像头初始化成功！" << std::endl;
        return true;
    }

    bool start()
    {
        if (!camera_->startPreview())
        {
            std::cerr << "启动预览失败！" << std::endl;
            return false;
        }

        running_ = true;
        processingThread_ = std::thread(&VideoAnalysisApp::processingLoop, this);

        std::cout << "视频分析开始运行..." << std::endl;
        return true;
    }

    void stop()
    {
        running_ = false;

        if (processingThread_.joinable())
        {
            processingThread_.join();
        }

        if (camera_)
        {
            camera_->stopPreview();
            camera_->cleanup();
        }

        if (recorder_ && recorder_->isRecording())
        {
            recorder_->stopRecording();
        }

        std::cout << "应用程序已停止" << std::endl;
    }

    void startRecording(const std::string &filename)
    {
        if (recorder_->startRecording(filename, cv::Size(1920, 1080), 25.0))
        {
            std::cout << "开始录制到文件: " << filename << std::endl;
        }
    }

private:
    void onFrameReceived(const cv::Mat &frame, int channel)
    {
        // 这是实时回调，在这里进行快速处理
        // 避免长时间阻塞，影响帧率

        if (frame.empty())
            return;

        // 可以在这里进行简单的预处理
        // 复杂的AI处理在单独线程中进行

        processedFrames_++;
    }

    void onError(const std::string &error)
    {
        std::cerr << "摄像头错误: " << error << std::endl;
        // 可以在这里实现自动重连逻辑
    }

    void onStatusUpdate(const PerformanceMetrics &metrics)
    {
        // 定期输出性能指标
        static int updateCount = 0;
        if (++updateCount % 100 == 0)
        { // 每100次更新输出一次
            std::cout << "性能指标 - FPS1: " << metrics.fps1
                      << ", FPS2: " << metrics.fps2
                      << ", 连接状态: " << (metrics.isConnected ? "正常" : "断开")
                      << std::endl;
        }
    }

    void processingLoop()
    {
        cv::namedWindow("Channel 1 - Analysis", cv::WINDOW_AUTOSIZE);
        cv::namedWindow("Channel 2 - Analysis", cv::WINDOW_AUTOSIZE);
        cv::moveWindow("Channel 1 - Analysis", 100, 100);
        cv::moveWindow("Channel 2 - Analysis", 800, 100);

        while (running_)
        {
            // 获取最新帧
            cv::Mat frame1 = camera_->getFrame(0);
            cv::Mat frame2 = camera_->getFrame(1);

            // 处理通道1
            if (!frame1.empty())
            {
                processChannel(frame1, 0, "Channel 1 - Analysis");

                // 录制通道1（如果需要）
                if (recorder_->isRecording())
                {
                    recorder_->recordFrame(frame1);
                }
            }

            // 处理通道2
            if (!frame2.empty())
            {
                processChannel(frame2, 1, "Channel 2 - Analysis");
            }

            // 检查退出条件
            char key = cv::waitKey(1) & 0xFF;
            if (key == 27)
            { // ESC键
                running_ = false;
                break;
            }

            // 控制处理频率，避免CPU占用过高
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        cv::destroyAllWindows();
    }

    void processChannel(const cv::Mat &frame, int channel, const std::string &windowName)
    {
        // AI目标检测
        auto detections = aiProcessor_->detectObjects(frame);

        // 绘制检测结果
        cv::Mat result = aiProcessor_->drawDetections(frame, detections);

        // 添加性能信息
        addPerformanceInfo(result, channel);

        // 显示结果
        cv::imshow(windowName, result);

        // 处理检测结果
        handleDetections(detections, channel);
    }

    void addPerformanceInfo(cv::Mat &frame, int channel)
    {
        double fps = camera_->getFPS(channel);
        std::string fpsText = "FPS: " + std::to_string(static_cast<int>(fps));

        // 计算处理帧率
        auto now = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - lastMetricsTime_);
        if (duration.count() >= 1)
        {
            double processingFPS = processedFrames_ / static_cast<double>(duration.count());
            std::string procText = "Proc FPS: " + std::to_string(static_cast<int>(processingFPS));

            cv::putText(frame, fpsText, cv::Point(10, 30),
                        cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(0, 255, 0), 2);
            cv::putText(frame, procText, cv::Point(10, 60),
                        cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(255, 0, 0), 2);

            // 重置计数器
            processedFrames_ = 0;
            lastMetricsTime_ = now;
        }
        else
        {
            cv::putText(frame, fpsText, cv::Point(10, 30),
                        cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(0, 255, 0), 2);
        }
    }

    void handleDetections(const std::vector<AIProcessor::Detection> &detections, int channel)
    {
        // 处理检测结果的业务逻辑
        for (const auto &detection : detections)
        {
            if (detection.confidence > 0.8)
            {
                // 高置信度检测，可以触发报警或记录
                std::cout << "通道" << (channel + 1) << "检测到: "
                          << detection.className
                          << " (置信度: " << detection.confidence << ")" << std::endl;

                // 这里可以添加您的业务逻辑：
                // - 发送报警通知
                // - 保存检测图片
                // - 数据库记录
                // - 等等...
            }
        }
    }
};

// 简化的集成示例
class SimpleIntegrationExample
{
public:
    void runExample()
    {
        std::cout << "=== 简化集成示例 ===" << std::endl;

        HikCameraCapture camera;

        // 初始化
        if (!camera.initialize("192.168.1.64", "admin", "password", 8553))
        {
            std::cerr << "初始化失败！" << std::endl;
            return;
        }

        // 开始预览
        if (!camera.startPreview())
        {
            std::cerr << "启动预览失败！" << std::endl;
            return;
        }

        std::cout << "开始采集视频数据..." << std::endl;

        // 数据采集循环
        for (int i = 0; i < 1000; ++i)
        { // 采集1000帧
            // 获取帧数据
            cv::Mat frame1 = camera.getFrame(0);
            cv::Mat frame2 = camera.getFrame(1);

            // 您的后处理逻辑
            if (!frame1.empty())
            {
                processFrame(frame1, "Channel1");
            }

            if (!frame2.empty())
            {
                processFrame(frame2, "Channel2");
            }

            // 控制采集频率
            std::this_thread::sleep_for(std::chrono::milliseconds(40)); // 25FPS
        }

        std::cout << "数据采集完成" << std::endl;
    }

private:
    void processFrame(const cv::Mat &frame, const std::string &source)
    {
        // 这里是您的后处理逻辑
        // 例如：图像增强、特征提取、AI推理等

        cv::Mat processed = frame.clone();

        // 示例：简单的边缘检测
        cv::Mat gray, edges;
        cv::cvtColor(processed, gray, cv::COLOR_BGR2GRAY);
        cv::Canny(gray, edges, 100, 200);

        // 保存或进一步处理
        // cv::imwrite(source + "_" + std::to_string(frameCount++) + ".jpg", edges);

        std::cout << source << " 帧处理完成, 尺寸: "
                  << frame.cols << "x" << frame.rows << std::endl;
    }
};

// 主函数
int main()
{
    try
    {
        std::cout << "=== 海康威视SDK集成示例 ===" << std::endl;

        // 选择运行模式
        std::cout << "选择运行模式:" << std::endl;
        std::cout << "1. 完整视频分析应用" << std::endl;
        std::cout << "2. 简化集成示例" << std::endl;
        std::cout << "请输入选择 (1 或 2): ";

        int choice;
        std::cin >> choice;

        if (choice == 1)
        {
            // 完整应用示例
            VideoAnalysisApp app;

            if (app.initialize("192.168.1.64", "admin", "tkytjsyjs111"))
            {
                if (app.start())
                {
                    std::cout << "按 Ctrl+C 或在视频窗口按ESC键退出..." << std::endl;

                    // 可选：开始录制
                    // app.startRecording("output.mp4");

                    // 等待用户输入退出
                    std::cin.get();
                    std::cin.get(); // 等待回车
                }
            }
        }
        else if (choice == 2)
        {
            // 简化示例
            SimpleIntegrationExample example;
            example.runExample();
        }
        else
        {
            std::cout << "无效选择" << std::endl;
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "程序异常: " << e.what() << std::endl;
        return -1;
    }

    return 0;
}

// 扩展示例：多线程处理
class MultithreadedProcessor
{
private:
    HikCameraCapture camera_;
    std::queue<cv::Mat> frameQueue1_, frameQueue2_;
    std::mutex queueMutex1_, queueMutex2_;
    std::condition_variable queueCV1_, queueCV2_;
    std::atomic<bool> running_{false};

public:
    void start()
    {
        running_ = true;

        // 启动处理线程
        std::thread processor1(&MultithreadedProcessor::processChannel1, this);
        std::thread processor2(&MultithreadedProcessor::processChannel2, this);

        // 启动数据采集
        while (running_)
        {
            cv::Mat frame1 = camera_.getFrame(0);
            cv::Mat frame2 = camera_.getFrame(1);

            if (!frame1.empty())
            {
                std::lock_guard<std::mutex> lock(queueMutex1_);
                frameQueue1_.push(frame1.clone());
                queueCV1_.notify_one();
            }

            if (!frame2.empty())
            {
                std::lock_guard<std::mutex> lock(queueMutex2_);
                frameQueue2_.push(frame2.clone());
                queueCV2_.notify_one();
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        processor1.join();
        processor2.join();
    }

private:
    void processChannel1()
    {
        while (running_)
        {
            std::unique_lock<std::mutex> lock(queueMutex1_);
            queueCV1_.wait(lock, [this]
                           { return !frameQueue1_.empty() || !running_; });

            if (!frameQueue1_.empty())
            {
                cv::Mat frame = frameQueue1_.front();
                frameQueue1_.pop();
                lock.unlock();

                // 处理通道1数据
                heavyProcessing(frame, 1);
            }
        }
    }

    void processChannel2()
    {
        while (running_)
        {
            std::unique_lock<std::mutex> lock(queueMutex2_);
            queueCV2_.wait(lock, [this]
                           { return !frameQueue2_.empty() || !running_; });

            if (!frameQueue2_.empty())
            {
                cv::Mat frame = frameQueue2_.front();
                frameQueue2_.pop();
                lock.unlock();

                // 处理通道2数据
                heavyProcessing(frame, 2);
            }
        }
    }

    void heavyProcessing(const cv::Mat &frame, int channel)
    {
        // 模拟重量级处理（AI推理、复杂算法等）
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        std::cout << "通道" << channel << "重量级处理完成" << std::endl;
    }
};