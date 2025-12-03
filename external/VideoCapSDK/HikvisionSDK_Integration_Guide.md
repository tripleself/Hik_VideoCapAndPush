# 海康威视SDK双通道视频采集集成指南

## 📋 概述

本文档详细说明了如何使用海康威视SDK进行双通道视频采集，并将其集成到您的应用程序中进行后续处理。该解决方案提供了高性能、低延迟的视频流获取能力。

## 🏗️ 系统架构

### 核心组件架构图
```
┌─────────────────┐    ┌──────────────────┐    ┌─────────────────┐
│   海康摄像头    │───▶│   HikCameraCapture   │───▶│   应用程序处理   │
│   (双通道)      │    │      SDK封装类      │    │    (后处理)      │
└─────────────────┘    └──────────────────┘    └─────────────────┘
                              │
                              ▼
                       ┌──────────────────┐
                       │   OpenCV显示     │
                       │   (可选组件)      │
                       └──────────────────┘
```

### 数据流程图
```
海康摄像头 → NET_DVR_RealPlay_V40 → dataCallback → PlayM4_InputData 
                                                         │
                                                         ▼
cv::Mat ← 颜色空间转换 ← decodeCallback ← PlayM4解码库
   │
   ▼
应用程序后处理 / OpenCV显示
```

## 🔧 核心技术解析

### 1. 多通道用户指针编码技术

**问题背景**：
- `RealDataCallback` 支持完整的64位用户指针
- `DecodeCallback` 只支持32位long类型用户参数
- 需要在两个回调间传递对象指针和通道信息

**解决方案**：
```cpp
// 编码：将对象指针和通道号打包
void* userData = reinterpret_cast<void*>(
    (static_cast<uintptr_t>(reinterpret_cast<uintptr_t>(this)) << 8) | channel
);

// 解码：分别提取对象指针和通道号
uintptr_t userValue = reinterpret_cast<uintptr_t>(pUser);
HikCameraCapture* camera = reinterpret_cast<HikCameraCapture*>(userValue >> 8);
int channel = static_cast<int>(userValue & 0xFF);
```

### 2. 全局端口映射表

为`DecodeCallback`提供对象查找：
```cpp
std::map<LONG, void*> g_portMap;  // 端口号 → 编码后的用户数据
std::mutex g_mapMutex;            // 线程安全保护
```

### 3. 线程安全的帧数据管理

```cpp
// 双通道独立的帧数据和互斥锁
std::mutex g_frameMutex1, g_frameMutex2;
cv::Mat g_currentFrame1, g_currentFrame2;

// 原子操作的帧率计数
std::atomic<int> g_frameCount1(0), g_frameCount2(0);
std::atomic<double> g_fps1(0.0), g_fps2(0.0);
```

## 🚀 集成到您的应用程序

### 1. 基本集成模式

```cpp
#include "HikCameraCapture.h"  // 您需要将类定义分离到头文件

class YourApplication {
private:
    HikCameraCapture* camera;
    std::thread processingThread;
    
public:
    bool initializeCamera() {
        camera = new HikCameraCapture();
        return camera->initialize("192.168.1.64", "admin", "password", 8553);
    }
    
    void startProcessing() {
        camera->startPreview();
        processingThread = std::thread(&YourApplication::processFrames, this);
    }
    
    void processFrames() {
        while (running) {
            cv::Mat frame1, frame2;
            
            // 获取通道1数据
            {
                std::lock_guard<std::mutex> lock(g_frameMutex1);
                if (!g_currentFrame1.empty()) {
                    frame1 = g_currentFrame1.clone();
                }
            }
            
            // 获取通道2数据  
            {
                std::lock_guard<std::mutex> lock(g_frameMutex2);
                if (!g_currentFrame2.empty()) {
                    frame2 = g_currentFrame2.clone();
                }
            }
            
            // 您的后处理逻辑
            if (!frame1.empty()) processChannel1(frame1);
            if (!frame2.empty()) processChannel2(frame2);
            
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
};
```

### 2. 回调式集成模式

```cpp
class YourApplication {
public:
    // 注册帧处理回调
    static void frameCallback(const cv::Mat& frame, int channel) {
        // 直接在解码回调中处理，减少延迟
        if (channel == 0) {
            // 处理通道1
            processChannel1Frame(frame);
        } else {
            // 处理通道2  
            processChannel2Frame(frame);
        }
    }
};

// 修改decodeCallback以支持自定义处理
static void CALLBACK decodeCallback(long nPort, char* pBuf, long nSize,
                                  FRAME_INFO* pFrameInfo, long nUser, long nReserved2) {
    // ... 现有解码逻辑 ...
    
    // 添加自定义回调
    if (frameProcessingCallback) {
        frameProcessingCallback(bgrMat, channel);
    }
}
```

## ⚙️ 配置与优化

### 1. 性能优化参数

```cpp
// 连接优化
NET_DVR_SetConnectTime(1000, 1);    // 连接超时1秒
NET_DVR_SetReconnect(5000, true);   // 重连间隔5秒

// 流模式优化
previewInfo.bBlocked = 0;           // 非阻塞模式

// 缓冲区优化
PlayM4_OpenStream(port, nullptr, 0, 512*1024);  // 512KB缓冲区
```

### 2. 内存管理

```cpp
// 及时释放资源
void cleanup() {
    // 停止预览
    stopPreview();
    
    // 清理播放库
    for (int i = 0; i < 2; i++) {
        if (m_playPort[i] >= 0) {
            PlayM4_Stop(m_playPort[i]);
            PlayM4_CloseStream(m_playPort[i]);  
            PlayM4_FreePort(m_playPort[i]);
        }
    }
    
    // 登出设备
    if (m_userId >= 0) {
        NET_DVR_Logout(m_userId);
    }
    
    // 清理SDK
    NET_DVR_Cleanup();
}
```

## 📊 实时性能监控

### 1. 帧率计算

```cpp
void calculateFPS() {
    static auto lastTime = std::chrono::steady_clock::now();
    static int frameCount = 0;
    
    if (++frameCount % 60 == 0) {
        auto currentTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            currentTime - lastTime);
            
        double fps = (frameCount * 1000.0) / duration.count();
        
        // 重置计数器
        frameCount = 0;
        lastTime = currentTime;
        
        // 输出或存储FPS
        onFPSUpdated(fps);
    }
}
```

### 2. 性能指标监控

```cpp
struct PerformanceMetrics {
    double fps1, fps2;                    // 双通道帧率
    size_t memoryUsage;                   // 内存使用量
    std::chrono::milliseconds latency;    // 端到端延迟
    int droppedFrames;                    // 丢帧数量
};
```

## 🔌 API接口说明

### HikCameraCapture 类接口

```cpp
class HikCameraCapture {
public:
    // 基本生命周期
    HikCameraCapture();
    ~HikCameraCapture();
    
    // 设备连接
    bool initialize(const std::string& ip, 
                   const std::string& username,
                   const std::string& password, 
                   int port = 8000);
    
    // 预览控制
    bool startPreview();
    void stopPreview();
    
    // 资源管理
    void cleanup();
    
    // 状态查询
    bool isConnected() const;
    PerformanceMetrics getMetrics() const;
    
    // 回调注册
    void setFrameCallback(std::function<void(const cv::Mat&, int)> callback);
    void setErrorCallback(std::function<void(const std::string&)> callback);
};
```

### 全局数据访问

```cpp
// 获取实时帧数据
cv::Mat getChannel1Frame() {
    std::lock_guard<std::mutex> lock(g_frameMutex1);
    return g_currentFrame1.clone();
}

cv::Mat getChannel2Frame() {
    std::lock_guard<std::mutex> lock(g_frameMutex2);
    return g_currentFrame2.clone();
}

// 获取性能数据
double getChannel1FPS() { return g_fps1.load(); }
double getChannel2FPS() { return g_fps2.load(); }
```

## 🛠️ CMake集成配置

```cmake
# 您的CMakeLists.txt
cmake_minimum_required(VERSION 3.16)
project(YourProject)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找OpenCV
find_package(OpenCV REQUIRED)

# 海康威视SDK路径
set(HIKVISION_SDK_ROOT "path/to/your/sdk")
set(HIKVISION_INCLUDE_DIR "${HIKVISION_SDK_ROOT}/include")
set(HIKVISION_LIB_DIR "${HIKVISION_SDK_ROOT}/lib")

# 包含目录
include_directories(${OpenCV_INCLUDE_DIRS})
include_directories(${HIKVISION_INCLUDE_DIR})

# 库目录
link_directories(${HIKVISION_LIB_DIR})

# 创建可执行文件
add_executable(YourProject 
    main.cpp
    HikCameraCapture.cpp  # 分离后的实现文件
)

# 链接库
target_link_libraries(YourProject
    ${OpenCV_LIBS}
    HCNetSDK
    PlayCtrl
    # 其他Windows库...
)
```

## ⚠️ 重要注意事项

### 1. 线程安全
- 所有帧数据访问都需要互斥锁保护
- 回调函数在SDK内部线程执行，避免长时间阻塞
- 使用原子变量进行计数和状态管理

### 2. 资源管理
- 确保在程序退出前调用cleanup()
- 避免内存泄漏，及时释放cv::Mat
- 监控端口和句柄的分配与释放

### 3. 错误处理
```cpp
// 检查SDK错误
if (result < 0) {
    DWORD error = NET_DVR_GetLastError();
    handleError("操作失败，错误码：" + std::to_string(error));
}

// 网络异常处理
void CALLBACK exceptionCallback(DWORD dwType, LONG lUserID, LONG lHandle, void* pUser) {
    switch (dwType) {
        case EXCEPTION_RECONNECT:
            // 处理重连逻辑
            break;
        case EXCEPTION_ALARMRECONNECT:
            // 处理报警重连
            break;
    }
}
```

### 4. 性能调优
```cpp
// 减少内存拷贝
void processFrame(const cv::Mat& frame) {
    // 避免不必要的clone()
    // 使用引用传递
    // 考虑就地处理
}

// 批量处理
void processBatch() {
    std::vector<cv::Mat> frames;
    // 收集多帧后批量处理
}
```

## 🐛 故障排除

### 常见问题及解决方案

| 问题 | 可能原因 | 解决方案 |
|------|----------|----------|
| 连接失败 | IP/端口/凭据错误 | 检查网络连接和设备配置 |
| 无图像显示 | 通道号错误 | 确认设备支持的通道数 |
| 帧率过低 | 网络带宽不足 | 调整码流参数或网络配置 |
| 内存泄漏 | 未正确释放资源 | 检查cleanup()调用 |
| 程序崩溃 | 线程安全问题 | 检查互斥锁使用 |

### 调试技巧

```cpp
// 启用详细日志
#define ENABLE_DEBUG_LOG
#ifdef ENABLE_DEBUG_LOG
    #define DEBUG_LOG(msg) std::cout << "[DEBUG] " << msg << std::endl;
#else
    #define DEBUG_LOG(msg)
#endif

// 性能监控
class PerformanceMonitor {
    std::chrono::high_resolution_clock::time_point start;
public:
    void startTiming() { start = std::chrono::high_resolution_clock::now(); }
    double getElapsedMs() {
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double, std::milli>(end - start).count();
    }
};
```

## 📈 扩展功能建议

### 1. 多设备支持
```cpp
class MultiDeviceManager {
    std::vector<std::unique_ptr<HikCameraCapture>> cameras;
public:
    void addDevice(const std::string& ip, const std::string& user, const std::string& pass);
    void startAllDevices();
    std::vector<cv::Mat> getAllFrames();
};
```

### 2. 录像功能
```cpp
class VideoRecorder {
    cv::VideoWriter writer;
public:
    bool startRecording(const std::string& filename, cv::Size frameSize, double fps);
    void recordFrame(const cv::Mat& frame);
    void stopRecording();
};
```

### 3. 智能分析集成
```cpp
class AIProcessor {
public:
    virtual void processFrame(const cv::Mat& frame, int channel) = 0;
    virtual std::vector<Detection> getDetections() = 0;
};

class ObjectDetector : public AIProcessor {
    // 实现目标检测逻辑
};
```

## 📝 版本信息

- **当前版本**: v1.0.0
- **兼容SDK**: 海康威视 SDK v6.1.9.48+
- **支持平台**: Windows 10/11 x64
- **依赖库**: OpenCV 4.5+, C++17

---

**作者**: [Your Name]  
**更新日期**: 2024年1月  
**联系方式**: [your.email@domain.com]

> 💡 **提示**: 建议将此代码模块化，分离成独立的库文件，便于在多个项目中复用。 