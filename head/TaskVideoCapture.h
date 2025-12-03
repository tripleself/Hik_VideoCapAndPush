#pragma once
#include <opencv2/opencv.hpp>
#include <thread>
#include <memory>
#include <unordered_map>
#include <atomic>
#include <mutex>
#include <map>
#include <vector>
#include <nlohmann/json.hpp>
#include "SharedData.h"

// 海康威视SDK头文件
#define WIN32_LEAN_AND_MEAN
#define _WINSOCKAPI_ // 防止winsock.h被包含
#include "HCNetSDK.h"
#include "plaympeg4.h"

/**
 * @brief 基于海康威视SDK的视频捕获任务类
 * 使用海康原生SDK拉流，支持双通道（热成像+可见光）
 * 支持1个摄像头（2路）和2个摄像头（4路）模式切换
 */
class TaskVideoCapture
{
public:
    /**
     * @brief 构造函数，初始化海康SDK视频捕获
     * @param cameraCount 摄像头数量（1或2）
     * @param deviceConfigs 设备配置列表
     * @param data 共享数据对象引用
     */
    TaskVideoCapture(int cameraCount, const std::vector<nlohmann::json> &deviceConfigs, SharedData &data);

    /**
     * @brief 析构函数，确保线程安全退出和资源清理
     */
    ~TaskVideoCapture();

    /**
     * @brief 启动视频捕获线程
     */
    void start();

    /**
     * @brief 停止视频捕获线程
     */
    void stop();

    /**
     * @brief 获取设备登录句柄
     */
    std::vector<LONG> getDeviceUserIDs() const { return userIDs_; }

private:
    /**
     * @brief 初始化海康SDK
     */
    bool initializeSDK();

    /**
     * @brief 登录设备
     */
    bool loginDevices();

    /**
     * @brief 配置设备的热成像参数
     * @param deviceIdx 设备索引
     * @return true if successful, false otherwise
     */
    bool configureThermometry(int deviceIdx);

    /**
     * @brief 配置热成像前端参数（AGC和调色板）
     * @param deviceIdx 设备索引
     * @param channel 通道号
     * @return true if successful, false otherwise
     */
    bool configureThermalCameraParams(int deviceIdx, LONG channel);

    /**
     * @brief 启动实时测温数据获取
     * @param deviceIdx 设备索引
     * @return true if successful, false otherwise
     */
    bool startRealtimeThermometry(int deviceIdx);

    /**
     * @brief 停止实时测温数据获取
     * @param deviceIdx 设备索引
     */
    void stopRealtimeThermometry(int deviceIdx);

    /**
     * @brief 初始化播放库
     */
    bool initializePlayback();

    /**
     * @brief 开始预览
     */
    bool startPreview();

    /**
     * @brief 停止预览
     */
    void stopPreview();

    /**
     * @brief 清理资源
     */
    void cleanup();

    /**
     * @brief 线程主函数
     */
    void run();

    /**
     * @brief 异常回调函数
     */
    static void CALLBACK exceptionCallback(DWORD dwType, LONG lUserID, LONG lHandle, void *pUser);

    /**
     * @brief 实时数据回调函数
     */
    static void CALLBACK dataCallback(LONG lPlayHandle, DWORD dwDataType,
                                      BYTE *pBuffer, DWORD dwBufSize, void *pUser);

    /**
     * @brief 解码回调函数
     */
    static void CALLBACK decodeCallback(long nPort, char *pBuf, long nSize,
                                        FRAME_INFO *pFrameInfo, long nUser, long nReserved2);

    /**
     * @brief 实时测温数据回调函数
     */
    static void CALLBACK thermometryCallback(DWORD dwType, void *lpBuffer, DWORD dwBufLen, void *pUserData);

private:
    int cameraCount_;                           // 摄像头数量（1或2）
    std::vector<nlohmann::json> deviceConfigs_; // 设备配置列表
    SharedData &data_;                          // 共享数据引用
    std::thread thread_;                        // 视频捕获线程

    // 海康SDK相关
    std::vector<LONG> userIDs_;                    // 设备登录句柄
    std::vector<bool> deviceLoginSuccess_;         // 设备登录成功状态[设备索引]
    std::vector<std::array<LONG, 2>> playHandles_; // 播放句柄[设备][通道]
    std::vector<std::array<LONG, 2>> playPorts_;   // 播放端口[设备][通道]

    // 实时测温相关
    std::vector<LONG> thermometryHandles_; // 实时测温句柄[设备]
    std::vector<bool> thermometryActive_;  // 测温状态标志[设备]

    // 端口到设备/通道映射
    static std::map<LONG, std::pair<int, int>> portMap_; // 端口 -> (设备索引, 通道索引)
    static std::mutex portMapMutex_;
    static TaskVideoCapture *instance_; // 静态实例指针，用于回调函数
    static std::mutex instanceMutex_;

    // 帧数据缓存
    std::vector<std::array<cv::Mat, 2>> frameBuffers_;                     // [设备][通道]
    std::vector<std::array<std::unique_ptr<std::mutex>, 2>> frameMutexes_; // [设备][通道]

    // SDK视频保存相关（基于NET_DVR_SaveRealData）
    std::vector<bool> videoSaveActive_;         // 视频保存状态[设备索引] (仅可见光通道)
    std::vector<std::thread> videoSaveThreads_; // 视频保存线程[设备索引]
    std::vector<LONG> videoSaveHandles_;        // 视频保存专用预览句柄[设备索引]
    std::atomic<bool> shouldStopVideoSave_;     // 视频保存停止信号
    std::thread storageMonitorThread_;          // 存储空间监控线程

    /**
     * @brief SDK视频保存线程函数
     * @param deviceIdx 设备索引 (0 or 1)
     */
    void sdkVideoSaveThread(int deviceIdx);

    /**
     * @brief 启动SDK视频保存
     * @param deviceIdx 设备索引
     * @return true if started successfully, false otherwise
     */
    bool startSDKVideoSave(int deviceIdx);

    /**
     * @brief 停止SDK视频保存
     * @param deviceIdx 设备索引
     */
    void stopSDKVideoSave(int deviceIdx);

    /**
     * @brief 生成视频文件名
     * @param deviceIdx 设备索引
     * @return 视频文件完整路径
     */
    std::string generateVideoFilePath(int deviceIdx);

    /**
     * @brief 配置SDK文件切片参数
     * @return true if configured successfully, false otherwise
     */
    bool configureSDKFileSplit();

    /**
     * @brief 存储空间监控线程函数
     * 定期检查存储空间，超过限制时删除旧文件
     */
    void storageMonitorThread();

    /**
     * @brief 计算目录总大小
     * @param path 目录路径
     * @return 目录大小（字节）
     */
    size_t calculateDirectorySize(const std::string &path);

    /**
     * @brief 清理旧视频文件
     * @param targetCleanupSize 目标清理大小（字节）
     * @return 实际清理的大小（字节）
     */
    size_t cleanupOldVideos(size_t targetCleanupSize);

    /**
     * @brief 获取按时间排序的视频文件列表
     * @return 视频文件列表（从旧到新）
     */
    std::vector<std::filesystem::path> getVideoFilesSortedByTime();
};