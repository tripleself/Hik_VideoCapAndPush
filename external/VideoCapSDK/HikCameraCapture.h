#ifndef HIK_CAMERA_CAPTURE_H
#define HIK_CAMERA_CAPTURE_H

#include <string>
#include <functional>
#include <opencv2/opencv.hpp>
#include <mutex>
#include <atomic>
#include <chrono>

// 前向声明，避免头文件依赖
struct FRAME_INFO;

// 性能监控结构体
struct PerformanceMetrics
{
    double fps1 = 0.0;
    double fps2 = 0.0;
    size_t memoryUsage = 0;
    std::chrono::milliseconds latency{0};
    int droppedFrames = 0;
    bool isConnected = false;
};

// 全局帧数据访问接口（线程安全）
namespace HikFrameData
{
    cv::Mat getChannel1Frame();
    cv::Mat getChannel2Frame();
    double getChannel1FPS();
    double getChannel2FPS();
    bool hasNewFrame1();
    bool hasNewFrame2();
}

/**
 * @brief 海康威视双通道摄像头捕获类
 *
 * 支持同时从两个通道获取视频流，提供高性能、低延迟的视频采集能力。
 * 线程安全，支持实时性能监控。
 */
class HikCameraCapture
{
public:
    // 回调函数类型定义
    using FrameCallback = std::function<void(const cv::Mat &frame, int channel)>;
    using ErrorCallback = std::function<void(const std::string &error)>;
    using StatusCallback = std::function<void(const PerformanceMetrics &metrics)>;

public:
    /**
     * @brief 构造函数
     */
    HikCameraCapture();

    /**
     * @brief 析构函数，自动清理资源
     */
    ~HikCameraCapture();

    /**
     * @brief 初始化摄像头连接
     * @param ip 摄像头IP地址
     * @param username 用户名
     * @param password 密码
     * @param port 端口号，默认8000
     * @return 成功返回true，失败返回false
     */
    bool initialize(const std::string &ip,
                    const std::string &username,
                    const std::string &password,
                    int port = 8000);

    /**
     * @brief 开始双通道预览
     * @return 成功返回true，失败返回false
     */
    bool startPreview();

    /**
     * @brief 停止预览
     */
    void stopPreview();

    /**
     * @brief 清理所有资源
     */
    void cleanup();

    /**
     * @brief 检查连接状态
     * @return 连接状态
     */
    bool isConnected() const;

    /**
     * @brief 获取性能指标
     * @return 性能指标结构体
     */
    PerformanceMetrics getMetrics() const;

    /**
     * @brief 设置帧处理回调函数
     * @param callback 帧回调函数，参数为(frame, channel)
     */
    void setFrameCallback(FrameCallback callback);

    /**
     * @brief 设置错误处理回调函数
     * @param callback 错误回调函数
     */
    void setErrorCallback(ErrorCallback callback);

    /**
     * @brief 设置状态监控回调函数
     * @param callback 状态回调函数
     */
    void setStatusCallback(StatusCallback callback);

    /**
     * @brief 获取指定通道的最新帧（线程安全）
     * @param channel 通道号 (0或1)
     * @return 帧数据，如果无数据返回空Mat
     */
    cv::Mat getFrame(int channel);

    /**
     * @brief 获取指定通道的帧率
     * @param channel 通道号 (0或1)
     * @return 帧率值
     */
    double getFPS(int channel);

    /**
     * @brief 设置性能优化参数
     * @param connectTimeout 连接超时时间(ms)
     * @param reconnectInterval 重连间隔(ms)
     * @param bufferSize 缓冲区大小(bytes)
     * @param nonBlocking 是否使用非阻塞模式
     */
    void setPerformanceParams(int connectTimeout = 1000,
                              int reconnectInterval = 5000,
                              int bufferSize = 512 * 1024,
                              bool nonBlocking = true);

private:
    // 禁止拷贝构造和赋值
    HikCameraCapture(const HikCameraCapture &) = delete;
    HikCameraCapture &operator=(const HikCameraCapture &) = delete;

    // 私有成员变量
    struct Impl;
    std::unique_ptr<Impl> pImpl; // PIMPL习惯用法，隐藏实现细节

    // 静态回调函数
    static void CALLBACK exceptionCallback(unsigned long dwType, long lUserID, long lHandle, void *pUser);
    static void CALLBACK dataCallback(long lPlayHandle, unsigned long dwDataType,
                                      unsigned char *pBuffer, unsigned long dwBufSize, void *pUser);
    static void CALLBACK decodeCallback(long nPort, char *pBuf, long nSize,
                                        FRAME_INFO *pFrameInfo, long nUser, long nReserved2);
};

/**
 * @brief 多设备管理器
 *
 * 用于管理多个海康摄像头设备
 */
class MultiDeviceManager
{
public:
    /**
     * @brief 添加设备
     * @param deviceId 设备标识
     * @param ip IP地址
     * @param username 用户名
     * @param password 密码
     * @param port 端口号
     * @return 成功返回true
     */
    bool addDevice(const std::string &deviceId,
                   const std::string &ip,
                   const std::string &username,
                   const std::string &password,
                   int port = 8000);

    /**
     * @brief 启动所有设备
     * @return 成功启动的设备数量
     */
    int startAllDevices();

    /**
     * @brief 停止所有设备
     */
    void stopAllDevices();

    /**
     * @brief 获取所有设备的帧数据
     * @return 设备ID到帧数据对的映射
     */
    std::map<std::string, std::vector<cv::Mat>> getAllFrames();

    /**
     * @brief 获取设备数量
     * @return 设备数量
     */
    size_t getDeviceCount() const;

private:
    std::map<std::string, std::unique_ptr<HikCameraCapture>> devices_;
    mutable std::mutex devicesMutex_;
};

/**
 * @brief 视频录制器
 *
 * 用于录制海康摄像头的视频流
 */
class VideoRecorder
{
public:
    VideoRecorder();
    ~VideoRecorder();

    /**
     * @brief 开始录制
     * @param filename 输出文件名
     * @param frameSize 帧大小
     * @param fps 帧率
     * @param codec 编解码器 (默认H.264)
     * @return 成功返回true
     */
    bool startRecording(const std::string &filename,
                        cv::Size frameSize,
                        double fps,
                        int codec = cv::VideoWriter::fourcc('H', '2', '6', '4'));

    /**
     * @brief 录制一帧
     * @param frame 帧数据
     */
    void recordFrame(const cv::Mat &frame);

    /**
     * @brief 停止录制
     */
    void stopRecording();

    /**
     * @brief 检查是否正在录制
     * @return 录制状态
     */
    bool isRecording() const;

private:
    cv::VideoWriter writer_;
    mutable std::mutex writerMutex_;
    std::atomic<bool> recording_{false};
};

// 实用工具函数
namespace HikUtils
{
    /**
     * @brief 获取SDK错误信息
     * @param errorCode 错误码
     * @return 错误描述字符串
     */
    std::string getErrorString(unsigned long errorCode);

    /**
     * @brief 检查网络连通性
     * @param ip IP地址
     * @param port 端口
     * @param timeoutMs 超时时间(ms)
     * @return 连通性状态
     */
    bool checkNetworkConnectivity(const std::string &ip, int port, int timeoutMs = 3000);

    /**
     * @brief 格式化性能指标为字符串
     * @param metrics 性能指标
     * @return 格式化的字符串
     */
    std::string formatMetrics(const PerformanceMetrics &metrics);

    /**
     * @brief 计算两个时间点之间的延迟
     * @param start 开始时间点
     * @param end 结束时间点
     * @return 延迟(毫秒)
     */
    double calculateLatency(const std::chrono::steady_clock::time_point &start,
                            const std::chrono::steady_clock::time_point &end);
}

#endif // HIK_CAMERA_CAPTURE_H