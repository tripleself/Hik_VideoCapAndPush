#include <iostream>
#include <string>
#include <filesystem>
#include <opencv2/opencv.hpp>
#include "infer.h"
#include "tracker.h"
#include "counting_line.h"
#include "config.h"

// 简单的目标检测示例
void simpleDetectionExample()
{
    std::cout << "=== 简单目标检测示例 ===" << std::endl;

    // 初始化检测器
    std::string engine_path = "./models/yolo11n.plan";
    YoloDetector detector(engine_path, 0, 0.45f, 0.25f, 80);

    // 加载测试图像
    cv::Mat img = cv::imread("./test_image.jpg");
    if (img.empty())
    {
        std::cout << "无法加载测试图像！" << std::endl;
        return;
    }

    // 执行检测
    auto start = std::chrono::high_resolution_clock::now();
    std::vector<Detection> detections = detector.inference(img);
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    std::cout << "检测时间: " << duration.count() << " ms" << std::endl;
    std::cout << "检测到 " << detections.size() << " 个目标" << std::endl;

    // 绘制结果
    YoloDetector::draw_image(img, detections);

    // 保存结果
    cv::imwrite("./detection_result.jpg", img);
    std::cout << "检测结果已保存到: detection_result.jpg" << std::endl;
}

// 视频追踪示例（带虚拟检测线计数）
void videoTrackingExample()
{
    std::cout << "\n=== 视频目标追踪示例（带虚拟检测线计数） ===" << std::endl;

    // 初始化检测器和追踪器
    std::string engine_path = "./models/yolo11n.plan";
    YoloDetector detector(engine_path, 0, 0.45f, 0.25f, 80);
    TrackerModule tracker(30, 30, false, 0); // 30fps, 30帧缓冲, 不使用ReID, 追踪人员

    // 打开视频文件
    cv::VideoCapture cap("./test_video.mp4");
    if (!cap.isOpened())
    {
        std::cout << "无法打开视频文件！" << std::endl;
        return;
    }

    // 获取视频信息
    int frame_width = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_WIDTH));
    int frame_height = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_HEIGHT));
    double fps = cap.get(cv::CAP_PROP_FPS);

    std::cout << "视频信息: " << frame_width << "x" << frame_height
              << ", " << fps << " fps" << std::endl;

    // 初始化计数模块
    CountingLineModule counting_module(frame_width, frame_height, fps, "test_video.mp4");

    // 创建输出视频
    cv::VideoWriter writer("./tracking_result_with_counting.mp4",
                           cv::VideoWriter::fourcc('m', 'p', '4', 'v'),
                           fps, cv::Size(frame_width, frame_height));

    if (!writer.isOpened())
    {
        std::cout << "无法创建输出视频文件！" << std::endl;
        return;
    }

    // 设置计数记录文件并开启计数
    std::string count_file = "counting_results_example.txt";
    counting_module.setCountingFile(count_file);
    counting_module.startCounting();

    cv::Mat frame;
    int frame_count = 0;
    double total_detect_time = 0, total_track_time = 0;

    while (cap.read(frame))
    {
        frame_count++;
        auto frame_start_time = std::chrono::high_resolution_clock::now();

        // 计算当前帧的时间戳
        double current_frame_time_ms = (frame_count - 1) * (1000.0 / fps);

        // 执行检测
        auto detect_start = std::chrono::high_resolution_clock::now();
        std::vector<Detection> detections = detector.inference(frame);
        auto detect_end = std::chrono::high_resolution_clock::now();

        // 执行追踪
        auto track_start = std::chrono::high_resolution_clock::now();
        std::vector<TrackResult> tracks = tracker.update(detections);
        auto track_end = std::chrono::high_resolution_clock::now();

        // 计算处理时间
        auto detect_time = std::chrono::duration_cast<std::chrono::milliseconds>(detect_end - detect_start);
        auto track_time = std::chrono::duration_cast<std::chrono::milliseconds>(track_end - track_start);

        total_detect_time += detect_time.count();
        total_track_time += track_time.count();

        // 计算真实处理时间
        auto frame_end_time = std::chrono::high_resolution_clock::now();
        double real_processing_time_ms = std::chrono::duration<double, std::milli>(frame_end_time - frame_start_time).count();

        // 使用计数模块进行计数检测
        int new_crossings = counting_module.updateCounting(tracks, current_frame_time_ms, real_processing_time_ms);

        // 绘制虚拟检测线
        counting_module.drawDetectionLine(frame);

        // 绘制追踪结果
        TrackerModule::drawTrackResults(frame, tracks);

        // 显示性能信息
        std::string info = "Frame: " + std::to_string(frame_count) +
                           " | Detect: " + std::to_string(detect_time.count()) + "ms" +
                           " | Track: " + std::to_string(track_time.count()) + "ms" +
                           " | Tracks: " + std::to_string(tracks.size());
        cv::putText(frame, info, cv::Point(10, 60), cv::FONT_HERSHEY_SIMPLEX,
                    0.5, cv::Scalar(255, 255, 255), 1);

        // 添加帧时间信息
        std::string time_info = "Time: " + std::to_string(static_cast<int>(current_frame_time_ms)) + "ms";
        cv::putText(frame, time_info, cv::Point(10, 90),
                    cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(255, 255, 255), 1);

        // 写入输出视频
        writer.write(frame);

        // 进度提示
        if (frame_count % 30 == 0)
        {
            std::cout << "处理帧数: " << frame_count << std::endl;
        }
    }

    // 释放资源
    cap.release();
    writer.release();

    // 完成计数并写入统计信息
    counting_module.finishCounting(frame_count);

    // 显示统计信息
    std::cout << "处理完成！" << std::endl;
    std::cout << "总帧数: " << frame_count << std::endl;
    std::cout << "平均检测时间: " << (total_detect_time / frame_count) << " ms/frame" << std::endl;
    std::cout << "平均追踪时间: " << (total_track_time / frame_count) << " ms/frame" << std::endl;
    std::cout << "穿越检测线的目标数量: " << counting_module.getTotalCount() << std::endl;
    std::cout << "结果保存到: tracking_result_with_counting.mp4" << std::endl;
    std::cout << "计数记录保存到: " << count_file << std::endl;
}

// 实时摄像头追踪示例
void realtimeCameraTracking()
{
    std::cout << "\n=== 实时摄像头追踪示例 ===" << std::endl;

    // 初始化检测器和追踪器
    std::string engine_path = "./models/yolo11n.plan";
    YoloDetector detector(engine_path, 0, 0.45f, 0.25f, 80);
    TrackerModule tracker(30, 30, false, 0); // 追踪人员

    // 打开摄像头
    cv::VideoCapture cap(0);
    if (!cap.isOpened())
    {
        std::cout << "无法打开摄像头！" << std::endl;
        return;
    }

    // 设置摄像头分辨率
    cap.set(cv::CAP_PROP_FRAME_WIDTH, 640);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, 480);

    std::cout << "按 'q' 键退出实时追踪" << std::endl;

    cv::Mat frame;
    while (true)
    {
        if (!cap.read(frame))
        {
            std::cout << "无法读取摄像头画面！" << std::endl;
            break;
        }

        // 执行检测和追踪
        auto start = std::chrono::high_resolution_clock::now();
        std::vector<Detection> detections = detector.inference(frame);
        std::vector<TrackResult> tracks = tracker.update(detections);
        auto end = std::chrono::high_resolution_clock::now();

        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        // 绘制追踪结果
        TrackerModule::drawTrackResults(frame, tracks);

        // 显示性能信息
        std::string fps_text = "FPS: " + std::to_string(1000.0 / duration.count());
        std::string track_text = "Tracks: " + std::to_string(tracks.size());
        cv::putText(frame, fps_text, cv::Point(10, 30), cv::FONT_HERSHEY_SIMPLEX,
                    0.7, cv::Scalar(0, 255, 0), 2);
        cv::putText(frame, track_text, cv::Point(10, 60), cv::FONT_HERSHEY_SIMPLEX,
                    0.7, cv::Scalar(0, 255, 0), 2);

        // 显示画面
        cv::imshow("实时目标追踪", frame);

        // 检查退出键
        if (cv::waitKey(1) & 0xFF == 'q')
        {
            break;
        }
    }

    cap.release();
    cv::destroyAllWindows();
}

int main()
{
    try
    {
        std::cout << "YOLO TensorRT 目标检测与追踪测试程序" << std::endl;
        std::cout << "========================================" << std::endl;

        // 检查模型文件是否存在
        if (!std::filesystem::exists("./models/yolo11n.plan"))
        {
            std::cout << "错误：找不到模型文件 ./models/yolo11n.plan" << std::endl;
            std::cout << "请确保模型文件已正确放置在models目录中" << std::endl;
            return -1;
        }

        // 选择要运行的示例
        int choice;
        std::cout << "请选择要运行的示例：" << std::endl;
        std::cout << "1. 简单目标检测" << std::endl;
        std::cout << "2. 视频目标追踪（虚拟检测线计数）" << std::endl;
        std::cout << "3. 实时摄像头追踪" << std::endl;
        std::cout << "请输入选择 (1-3): ";
        std::cin >> choice;

        switch (choice)
        {
        case 1:
            simpleDetectionExample();
            break;
        case 2:
            videoTrackingExample();
            break;
        case 3:
            realtimeCameraTracking();
            break;
        default:
            std::cout << "无效选择！" << std::endl;
            return -1;
        }
    }
    catch (const std::exception &e)
    {
        std::cout << "程序运行出错: " << e.what() << std::endl;
        return -1;
    }

    std::cout << "\n程序执行完成，按任意键退出..." << std::endl;
    std::cin.get();
    std::cin.get();
    return 0;
}