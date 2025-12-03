#include <stdio.h>
#include <iostream>
#include <windows.h>
#include <opencv2/opencv.hpp>
#include <chrono>

// 海康威视SDK头文件 - 注意包含顺序很重要
#define WIN32_LEAN_AND_MEAN
#define _WINSOCKAPI_ // 防止winsock.h被包含

#include "HCNetSDK.h"
#include "plaympeg4.h" // 或者 PlayM4.h，取决于SDK版本

#include <time.h>
#include <mutex>
#include <map>
#include <thread>
#include <atomic>

using namespace std;
using namespace cv;

// 全局变量用于线程间数据传递
std::mutex g_frameMutex1, g_frameMutex2;
cv::Mat g_currentFrame1, g_currentFrame2;
std::atomic<bool> g_running(true);
std::map<LONG, void *> g_portMap; // 端口到对象的映射
std::mutex g_mapMutex;

// 帧率计算相关
std::atomic<int> g_frameCount1(0), g_frameCount2(0);
std::chrono::steady_clock::time_point g_lastTime = std::chrono::steady_clock::now();
std::atomic<double> g_fps1(0.0), g_fps2(0.0);

// 前向声明
class HikCameraCapture;

// 全局变量指向摄像头对象
HikCameraCapture *g_camera = nullptr;

// 海康威视摄像头捕获类 - 支持双通道
class HikCameraCapture
{
public:
    HikCameraCapture() : m_userId(-1)
    {
        // 初始化双通道数据
        for (int i = 0; i < 2; i++)
        {
            m_playHandle[i] = -1;
            m_playPort[i] = -1;
        }
    }

    ~HikCameraCapture()
    {
        cleanup();
    }

    // 初始化SDK并登录设备
    bool initialize(const std::string &ip, const std::string &username,
                    const std::string &password, int port = 8000)
    {
        // 初始化SDK
        if (!NET_DVR_Init())
        {
            std::cout << "SDK初始化失败，错误码：" << NET_DVR_GetLastError() << std::endl;
            return false;
        }
        std::cout << "SDK初始化成功" << std::endl;

        // 设置连接时间与重连时间 - 优化实时性
        NET_DVR_SetConnectTime(1000, 1);  // 减少连接超时时间
        NET_DVR_SetReconnect(5000, true); // 减少重连间隔

        // 设置异常回调
        NET_DVR_SetExceptionCallBack_V30(0, NULL, exceptionCallback, NULL);

        // 登录设备
        NET_DVR_USER_LOGIN_INFO loginInfo = {0};
        loginInfo.bUseAsynLogin = 0;
        strncpy_s(loginInfo.sDeviceAddress, ip.c_str(), NET_DVR_DEV_ADDRESS_MAX_LEN);
        strncpy_s(loginInfo.sUserName, username.c_str(), NAME_LEN);
        strncpy_s(loginInfo.sPassword, password.c_str(), PASSWD_LEN);
        loginInfo.wPort = static_cast<WORD>(port);

        NET_DVR_DEVICEINFO_V40 deviceInfo;
        memset(&deviceInfo, 0, sizeof(NET_DVR_DEVICEINFO_V40));

        m_userId = NET_DVR_Login_V40(&loginInfo, &deviceInfo);

        if (m_userId < 0)
        {
            std::cout << "设备登录失败，错误码：" << NET_DVR_GetLastError() << std::endl;
            NET_DVR_Cleanup();
            return false;
        }
        std::cout << "设备登录成功，用户ID：" << m_userId << std::endl;

        // 初始化双通道播放库
        if (!initPlayback())
        {
            NET_DVR_Logout(m_userId);
            NET_DVR_Cleanup();
            return false;
        }

        return true;
    }

    // 初始化双通道播放库
    bool initPlayback()
    {
        for (int channel = 0; channel < 2; channel++)
        {
            // 获取播放端口
            if (!PlayM4_GetPort(&m_playPort[channel]))
            {
                std::cout << "通道" << (channel + 1) << "获取播放端口失败" << std::endl;
                return false;
            }
            std::cout << "通道" << (channel + 1) << "获取播放端口成功：" << m_playPort[channel] << std::endl;

            // 将端口与通道信息关联 - 这是关键！解决回调函数用户指针问题
            {
                std::lock_guard<std::mutex> lock(g_mapMutex);
                // 存储指向当前对象的指针和通道信息
                g_portMap[m_playPort[channel]] = reinterpret_cast<void *>((static_cast<uintptr_t>(reinterpret_cast<uintptr_t>(this)) << 8) | channel);
            }

            // 设置播放模式为实时流模式 - 优化延迟
            if (!PlayM4_SetStreamOpenMode(m_playPort[channel], STREAME_REALTIME))
            {
                std::cout << "通道" << (channel + 1) << "设置流模式失败" << std::endl;
                return false;
            }

            // 打开流 - 减少缓冲区大小以降低延迟
            if (!PlayM4_OpenStream(m_playPort[channel], nullptr, 0, 512 * 1024)) // 减少缓冲区
            {
                std::cout << "通道" << (channel + 1) << "打开流失败" << std::endl;
                return false;
            }

            // 设置解码回调
            if (!PlayM4_SetDecCallBackExMend(m_playPort[channel], decodeCallback, 0, 0, 0))
            {
                std::cout << "通道" << (channel + 1) << "设置解码回调失败" << std::endl;
                return false;
            }

            // 开始播放
            if (!PlayM4_Play(m_playPort[channel], nullptr))
            {
                std::cout << "通道" << (channel + 1) << "开始播放失败" << std::endl;
                return false;
            }

            std::cout << "通道" << (channel + 1) << "播放库初始化成功" << std::endl;
        }

        return true;
    }

    // 开始双通道预览
    bool startPreview()
    {
        for (int channel = 0; channel < 2; channel++)
        {
            NET_DVR_PREVIEWINFO previewInfo;
            memset(&previewInfo, 0, sizeof(NET_DVR_PREVIEWINFO));

            previewInfo.hPlayWnd = nullptr;     // 不需要窗口句柄，我们用回调
            previewInfo.lChannel = channel + 1; // 通道号（1和2）
            previewInfo.dwStreamType = 0;       // 主码流
            previewInfo.dwLinkMode = 0;         // TCP模式
            previewInfo.bBlocked = 0;           // 非阻塞模式 - 提高实时性

            // 开始实时预览，注册数据回调
            m_playHandle[channel] = NET_DVR_RealPlay_V40(m_userId, &previewInfo, dataCallback,
                                                         reinterpret_cast<void *>((static_cast<uintptr_t>(reinterpret_cast<uintptr_t>(this)) << 8) | channel));

            if (m_playHandle[channel] < 0)
            {
                std::cout << "通道" << (channel + 1) << "开始预览失败，错误码：" << NET_DVR_GetLastError() << std::endl;
                return false;
            }

            std::cout << "通道" << (channel + 1) << "开始预览成功，播放句柄：" << m_playHandle[channel] << std::endl;
        }

        return true;
    }

    // 停止双通道预览
    void stopPreview()
    {
        for (int channel = 0; channel < 2; channel++)
        {
            if (m_playHandle[channel] >= 0)
            {
                NET_DVR_StopRealPlay(m_playHandle[channel]);
                m_playHandle[channel] = -1;
                std::cout << "通道" << (channel + 1) << "停止预览" << std::endl;
            }
        }
    }

    // 清理资源
    void cleanup()
    {
        stopPreview();

        // 清理双通道播放库资源
        for (int channel = 0; channel < 2; channel++)
        {
            if (m_playPort[channel] >= 0)
            {
                PlayM4_Stop(m_playPort[channel]);
                PlayM4_CloseStream(m_playPort[channel]);
                PlayM4_FreePort(m_playPort[channel]);

                // 从映射中移除 - 防止野指针
                {
                    std::lock_guard<std::mutex> lock(g_mapMutex);
                    g_portMap.erase(m_playPort[channel]);
                }

                m_playPort[channel] = -1;
                std::cout << "通道" << (channel + 1) << "释放播放端口" << std::endl;
            }
        }

        // 登出设备
        if (m_userId >= 0)
        {
            NET_DVR_Logout(m_userId);
            m_userId = -1;
            std::cout << "设备登出" << std::endl;
        }

        // 清理SDK
        NET_DVR_Cleanup();
        std::cout << "SDK清理完成" << std::endl;
    }

private:
    LONG m_userId;        // 用户登录ID
    LONG m_playHandle[2]; // 双通道预览句柄
    LONG m_playPort[2];   // 双通道播放端口

    // 异常回调函数
    static void CALLBACK exceptionCallback(DWORD dwType, LONG lUserID, LONG lHandle, void *pUser)
    {
        switch (dwType)
        {
        case EXCEPTION_RECONNECT:
            std::cout << "预览重连，时间：" << time(NULL) << std::endl;
            break;
        default:
            std::cout << "异常类型：" << dwType << std::endl;
            break;
        }
    }

    // 实时数据回调函数
    static void CALLBACK dataCallback(LONG lPlayHandle, DWORD dwDataType,
                                      BYTE *pBuffer, DWORD dwBufSize, void *pUser)
    {
        // 解析用户数据获取对象指针和通道信息
        uintptr_t userValue = reinterpret_cast<uintptr_t>(pUser);
        HikCameraCapture *camera = reinterpret_cast<HikCameraCapture *>(userValue >> 8);
        int channel = static_cast<int>(userValue & 0xFF);

        // 只处理视频流数据
        if (dwDataType != NET_DVR_STREAMDATA || !camera || !pBuffer || dwBufSize == 0 || channel > 1)
        {
            return;
        }

        // 将数据送入播放库进行解码
        if (camera->m_playPort[channel] >= 0)
        {
            PlayM4_InputData(camera->m_playPort[channel], pBuffer, dwBufSize);
        }
    }

    // 解码回调函数：将YUV数据转换为BGR格式的Mat
    static void CALLBACK decodeCallback(long nPort, char *pBuf, long nSize,
                                        FRAME_INFO *pFrameInfo, long nUser, long nReserved2)
    {
        // 通过端口查找对应的摄像头对象和通道
        HikCameraCapture *camera = nullptr;
        int channel = -1;
        {
            std::lock_guard<std::mutex> lock(g_mapMutex);
            auto it = g_portMap.find(nPort);
            if (it != g_portMap.end())
            {
                uintptr_t userValue = reinterpret_cast<uintptr_t>(it->second);
                camera = reinterpret_cast<HikCameraCapture *>(userValue >> 8);
                channel = static_cast<int>(userValue & 0xFF);
            }
        }

        if (!camera || !pBuf || nSize <= 0 || !pFrameInfo || channel < 0 || channel > 1)
        {
            return;
        }

        // 处理YV12格式的YUV数据
        if (pFrameInfo->nType == T_YV12)
        {
            try
            {
                // 创建YUV Mat
                cv::Mat yuvMat(pFrameInfo->nHeight + pFrameInfo->nHeight / 2,
                               pFrameInfo->nWidth, CV_8UC1, (uchar *)pBuf);

                // 转换为BGR格式
                cv::Mat bgrMat;
                cv::cvtColor(yuvMat, bgrMat, cv::COLOR_YUV2BGR_YV12);

                // 根据通道更新对应的全局帧数据
                if (channel == 0)
                {
                    {
                        std::lock_guard<std::mutex> lock(g_frameMutex1);
                        g_currentFrame1 = bgrMat.clone();
                    }
                    g_frameCount1++;
                }
                else if (channel == 1)
                {
                    {
                        std::lock_guard<std::mutex> lock(g_frameMutex2);
                        g_currentFrame2 = bgrMat.clone();
                    }
                    g_frameCount2++;
                }

                // 每60帧计算一次帧率
                static int totalFrames = 0;
                if (++totalFrames % 60 == 0)
                {
                    auto currentTime = std::chrono::steady_clock::now();
                    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - g_lastTime);
                    if (duration.count() > 0)
                    {
                        g_fps1 = (g_frameCount1.load() * 1000.0) / duration.count();
                        g_fps2 = (g_frameCount2.load() * 1000.0) / duration.count();

                        // 重置计数器
                        g_frameCount1 = 0;
                        g_frameCount2 = 0;
                        g_lastTime = currentTime;
                    }
                }
            }
            catch (const cv::Exception &e)
            {
                std::cout << "OpenCV异常：" << e.what() << std::endl;
            }
            catch (...)
            {
                std::cout << "解码回调异常" << std::endl;
            }
        }
    }
};

// 双通道显示线程函数
void displayThread()
{
    // 创建两个窗口
    cv::namedWindow("Channel 1 - Hikvision Camera", cv::WINDOW_AUTOSIZE);
    cv::namedWindow("Channel 2 - Hikvision Camera", cv::WINDOW_AUTOSIZE);

    // 设置窗口位置
    cv::moveWindow("Channel 1 - Hikvision Camera", 100, 100);
    cv::moveWindow("Channel 2 - Hikvision Camera", 800, 100);

    while (g_running.load())
    {
        cv::Mat frame1, frame2;

        // 获取两个通道的帧数据
        {
            std::lock_guard<std::mutex> lock1(g_frameMutex1);
            if (!g_currentFrame1.empty())
            {
                frame1 = g_currentFrame1.clone();
            }
        }

        {
            std::lock_guard<std::mutex> lock2(g_frameMutex2);
            if (!g_currentFrame2.empty())
            {
                frame2 = g_currentFrame2.clone();
            }
        }

        // 显示通道1
        if (!frame1.empty())
        {
            // 在图像上绘制FPS信息
            cv::Mat displayFrame1 = frame1.clone();
            std::string fps_text = "Channel 1 FPS: " + std::to_string(static_cast<int>(g_fps1.load()));
            cv::putText(displayFrame1, fps_text, cv::Point(10, 30),
                        cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(0, 255, 0), 2);
            cv::imshow("Channel 1 - Hikvision Camera", displayFrame1);
        }
        else
        {
            cv::Mat waitMat1 = cv::Mat::zeros(480, 640, CV_8UC3);
            cv::putText(waitMat1, "Waiting for Channel 1...",
                        cv::Point(50, 240), cv::FONT_HERSHEY_SIMPLEX, 1,
                        cv::Scalar(0, 255, 0), 2);
            cv::imshow("Channel 1 - Hikvision Camera", waitMat1);
        }

        // 显示通道2
        if (!frame2.empty())
        {
            // 在图像上绘制FPS信息
            cv::Mat displayFrame2 = frame2.clone();
            std::string fps_text = "Channel 2 FPS: " + std::to_string(static_cast<int>(g_fps2.load()));
            cv::putText(displayFrame2, fps_text, cv::Point(10, 30),
                        cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(0, 255, 0), 2);
            cv::imshow("Channel 2 - Hikvision Camera", displayFrame2);
        }
        else
        {
            cv::Mat waitMat2 = cv::Mat::zeros(480, 640, CV_8UC3);
            cv::putText(waitMat2, "Waiting for Channel 2...",
                        cv::Point(50, 240), cv::FONT_HERSHEY_SIMPLEX, 1,
                        cv::Scalar(0, 255, 0), 2);
            cv::imshow("Channel 2 - Hikvision Camera", waitMat2);
        }

        // 按ESC退出 - 减少延迟
        char key = cv::waitKey(1) & 0xFF; // 改为1ms减少延迟
        if (key == 27)
        {
            g_running.store(false);
            break;
        }
    }

    cv::destroyAllWindows();
}

int main()
{
    std::cout << "=== 海康威视双通道摄像头 + OpenCV 显示程序 ===" << std::endl;

    // 创建摄像头对象
    g_camera = new HikCameraCapture();

    // 初始化摄像头（请修改为您的摄像头参数）
    std::string ip = "192.168.1.64";       // 摄像头IP地址
    std::string username = "admin";        // 用户名
    std::string password = "tkytjsyjs111"; // 密码
    int port = 8553;                       // 端口

    std::cout << "正在连接摄像头 " << ip << ":" << port << std::endl;

    if (!g_camera->initialize(ip, username, password, port))
    {
        std::cout << "摄像头初始化失败！" << std::endl;
        delete g_camera;
        return -1;
    }

    std::cout << "摄像头初始化成功，开始双通道预览..." << std::endl;

    if (!g_camera->startPreview())
    {
        std::cout << "开始预览失败！" << std::endl;
        delete g_camera;
        return -1;
    }

    std::cout << "双通道预览已开始，启动显示窗口..." << std::endl;
    std::cout << "按ESC键退出程序" << std::endl;

    // 启动显示线程
    std::thread display(displayThread);

    // 主线程等待
    display.join();

    std::cout << "程序退出，清理资源..." << std::endl;

    // 清理资源
    g_running.store(false);
    delete g_camera;
    g_camera = nullptr;

    std::cout << "程序结束" << std::endl;
    return 0;
}
