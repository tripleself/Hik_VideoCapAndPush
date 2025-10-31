#pragma once
#include <string>
#include <memory>
#include <iostream>
#include <fstream>
#include <nlohmann/json.hpp>
#include "config_manager.h"

/**
 * @brief 目标追踪系统配置适配器类
 * 负责加载配置文件并创建ConfigManager实例
 * 通过ConfigManager统一管理YOLO检测、ByteTrack追踪、计数模块的参数
 */
struct ObjectTrackingConfig
{
    // ========== 核心配置管理器 ==========
    std::shared_ptr<ConfigManager> configManager; // ConfigManager实例（统一管理所有追踪参数）

    // ========== 视频处理配置 ==========
    int videoWidth = 1920;  // 视频宽度（像素）
    int videoHeight = 1080; // 视频高度（像素）
    int processingFps = 25; // 处理帧率（fps）

    // ========== 显示配置 ==========
    bool enableDisplay = false;                 // 是否启用实时显示窗口
    std::string windowName = "Object Tracking"; // 显示窗口名称
    int windowWidth = 800;                      // 显示窗口宽度
    int windowHeight = 600;                     // 显示窗口高度

    // ========== 性能优化配置 ==========
    int threadSleepMs = 10;              // 线程休眠时间（毫秒，控制CPU使用率）
    bool enablePerformanceStats = false; // 是否启用性能统计显示

    // ========== 定位上报配置 ==========
    bool enableLocationReport = true; // 是否启用定位上报功能
    int tcpServerPort = 12346;        // TCP服务器端口
    int checkIntervalMs = 100;        // 检查间隔（毫秒）

    // ========== RS422串口配置 ==========
    struct RS422PortConfig
    {
        std::string portName = "COM1"; // 串口名称
        int baudRate = 9600;           // 波特率
        int dataBits = 8;              // 数据位
        int stopBits = 1;              // 停止位
        int parity = 0;                // 奇偶校验（0=无校验）
        int timeout = 1000;            // 读取超时时间（毫秒）
    } rs422Port;

    /**
     * @brief 从JSON配置对象加载所有参数
     */
    bool loadFromJson(const nlohmann::json &config)
    {
        try
        {
            if (!config.contains("object_tracking"))
            {
                std::cerr << "[ObjectTrackingConfig] 配置文件中未找到object_tracking节点" << std::endl;
                return false;
            }

            const auto &tracking = config["object_tracking"];

            // 直接加载tracking_config.json（独立的追踪配置文件）
            configManager = std::make_shared<ConfigManager>("tracking_config.json");
            if (!configManager->loadConfig())
            {
                std::cerr << "[ObjectTrackingConfig] ConfigManager加载tracking_config.json失败" << std::endl;
                return false;
            }

            // 加载视频处理配置
            if (tracking.contains("video_processing"))
            {
                const auto &video = tracking["video_processing"];
                videoWidth = video.value("video_width", videoWidth);
                videoHeight = video.value("video_height", videoHeight);
                processingFps = video.value("processing_fps", processingFps);
            }

            // 加载显示配置
            if (tracking.contains("display"))
            {
                const auto &display = tracking["display"];
                enableDisplay = display.value("enable_display", enableDisplay);
                windowName = display.value("window_name", windowName);
                windowWidth = display.value("window_width", windowWidth);
                windowHeight = display.value("window_height", windowHeight);
            }

            // 加载性能配置
            if (tracking.contains("performance"))
            {
                const auto &performance = tracking["performance"];
                threadSleepMs = performance.value("thread_sleep_ms", threadSleepMs);
                enablePerformanceStats = performance.value("enable_performance_stats", enablePerformanceStats);
            }

            // 加载定位上报配置
            if (tracking.contains("location_report"))
            {
                const auto &location = tracking["location_report"];
                enableLocationReport = location.value("enable_location_report", enableLocationReport);
                tcpServerPort = location.value("tcp_server_port", tcpServerPort);
                checkIntervalMs = location.value("check_interval_ms", checkIntervalMs);

                // 加载RS422串口配置
                if (location.contains("rs422_port"))
                {
                    const auto &rs422 = location["rs422_port"];
                    rs422Port.portName = rs422.value("port_name", rs422Port.portName);
                    rs422Port.baudRate = rs422.value("baud_rate", rs422Port.baudRate);
                    rs422Port.dataBits = rs422.value("data_bits", rs422Port.dataBits);
                    rs422Port.stopBits = rs422.value("stop_bits", rs422Port.stopBits);
                    rs422Port.parity = rs422.value("parity", rs422Port.parity);
                    rs422Port.timeout = rs422.value("timeout", rs422Port.timeout);
                }
            }

            std::cout << "[ObjectTrackingConfig] 配置参数加载成功" << std::endl;
            printConfig();
            return true;
        }
        catch (const std::exception &e)
        {
            std::cerr << "[ObjectTrackingConfig] 配置加载失败: " << e.what() << std::endl;
            return false;
        }
    }

    /**
     * @brief 验证配置参数的有效性
     */
    bool isValid() const
    {
        if (!configManager || !configManager->isValid())
        {
            std::cerr << "[ObjectTrackingConfig] ConfigManager未初始化或无效" << std::endl;
            return false;
        }

        if (videoWidth <= 0 || videoHeight <= 0)
        {
            std::cerr << "[ObjectTrackingConfig] 视频尺寸无效: " << videoWidth << "x" << videoHeight << std::endl;
            return false;
        }

        if (processingFps <= 0)
        {
            std::cerr << "[ObjectTrackingConfig] 处理帧率无效: " << processingFps << std::endl;
            return false;
        }

        return true;
    }

    /**
     * @brief 打印当前配置参数到控制台
     */
    void printConfig() const
    {
        std::cout << "\n========== 目标追踪配置参数 ==========\n";
        if (configManager)
        {
            std::cout << "模型路径: " << configManager->getEnginePath() << "\n";
            std::cout << "GPU设备ID: " << configManager->getGpuId() << "\n";
            std::cout << "类别数量: " << configManager->getNumClass() << "\n";
            std::cout << "置信度阈值: " << configManager->getConfidenceThreshold() << "\n";
            std::cout << "NMS阈值: " << configManager->getNmsThreshold() << "\n";
            std::cout << "追踪帧率: " << configManager->getFrameRate() << "\n";
            std::cout << "追踪缓冲: " << configManager->getTrackBuffer() << "\n";
            std::cout << "追踪类别: " << configManager->getTrackClass() << "\n";
            std::cout << "启用计数: " << (configManager->isCountingEnabled() ? "是" : "否") << "\n";
        }
        std::cout << "视频尺寸: " << videoWidth << "x" << videoHeight << "\n";
        std::cout << "处理帧率: " << processingFps << " fps\n";
        std::cout << "启用显示: " << (enableDisplay ? "是" : "否") << "\n";
        std::cout << "======================================\n\n";
    }

    /**
     * @brief 获取ConfigManager实例
     */
    std::shared_ptr<ConfigManager> getConfigManager() const
    {
        return configManager;
    }
};