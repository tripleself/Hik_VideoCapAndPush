#pragma once
#include "TaskVideoCapture.h"
#include "TaskThermalCapture.h"
#include "TaskDisplay.h"
#include "TaskRTSPStream.h"
#include "TaskLocating.h"
#include "TaskObjectTracking.h"
#include "TaskLocationReporter.h" // 包含目标追踪任务头文件
#include <memory>
#include <vector>
#include <nlohmann/json.hpp>

/**
 * @brief 线程管理类，负责统一管理所有任务线程
 * 支持1个摄像头（2路视频）和2个摄像头（4路视频）模式
 */
class ThreadManager
{
public:
    /**
     * @brief 构造函数，初始化所有任务线程
     * @param cameraCount 摄像头数量（1或2）
     * @param deviceConfigs 设备配置列表
     * @param sharedData 共享数据对象
     * @param rtspUrls 推流地址列表
     * @param trackingConfig 目标追踪配置
     * @param streamWidth RTSP推流宽度，0表示使用原始分辨率
     * @param streamHeight RTSP推流高度，0表示使用原始分辨率
     * @param streamFps RTSP推流帧率
     */
    ThreadManager(int cameraCount,
                  const std::vector<nlohmann::json> &deviceConfigs,
                  SharedData &sharedData,
                  const std::vector<std::string> &rtspUrls,
                  const ObjectTrackingConfig &trackingConfig = ObjectTrackingConfig{},
                  int streamWidth = 0,
                  int streamHeight = 0,
                  int streamFps = 25);

    /**
     * @brief 析构函数，确保所有线程安全停止
     */
    ~ThreadManager();

    /**
     * @brief 启动所有任务线程
     */
    void startAll();

    /**
     * @brief 停止所有任务线程
     */
    void stopAll();

private:
    SharedData &sharedData_;                                     // 共享数据引用
    std::unique_ptr<TaskVideoCapture> taskVideo_;                // 视频捕获任务（基于海康SDK）
    std::unique_ptr<TaskThermalCapture> taskThermal_;            // 热成像捕获任务
    std::unique_ptr<TaskDisplay> taskDisplay_;                   // 显示任务
    std::unique_ptr<TaskRTSPStream> taskRTSPStream_;             // RTSP推流任务
    std::unique_ptr<TaskLocating> taskLocating_;                 // 定位任务（只负责热成像检测）
    std::unique_ptr<TaskObjectTracking> taskObjectTracking_;     // 目标追踪任务（只负责可见光检测）
    std::unique_ptr<TaskLocationReporter> taskLocationReporter_; // 统一定位上报任务
};