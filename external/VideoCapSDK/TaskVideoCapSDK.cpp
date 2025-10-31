#include <stdio.h>
#include <iostream>
#include <windows.h>
#include <opencv2/opencv.hpp>
#include <chrono>

// ��������SDKͷ�ļ� - ע�����˳�����Ҫ
#define WIN32_LEAN_AND_MEAN
#define _WINSOCKAPI_ // ��ֹwinsock.h������

#include "HCNetSDK.h"
#include "plaympeg4.h" // ���� PlayM4.h��ȡ����SDK�汾

#include <time.h>
#include <mutex>
#include <map>
#include <thread>
#include <atomic>

using namespace std;
using namespace cv;

// ȫ�ֱ��������̼߳����ݴ���
std::mutex g_frameMutex1, g_frameMutex2;
cv::Mat g_currentFrame1, g_currentFrame2;
std::atomic<bool> g_running(true);
std::map<LONG, void *> g_portMap; // �˿ڵ������ӳ��
std::mutex g_mapMutex;

// ֡�ʼ������
std::atomic<int> g_frameCount1(0), g_frameCount2(0);
std::chrono::steady_clock::time_point g_lastTime = std::chrono::steady_clock::now();
std::atomic<double> g_fps1(0.0), g_fps2(0.0);

// ǰ������
class HikCameraCapture;

// ȫ�ֱ���ָ������ͷ����
HikCameraCapture *g_camera = nullptr;

// ������������ͷ������ - ֧��˫ͨ��
class HikCameraCapture
{
public:
    HikCameraCapture() : m_userId(-1)
    {
        // ��ʼ��˫ͨ������
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

    // ��ʼ��SDK����¼�豸
    bool initialize(const std::string &ip, const std::string &username,
                    const std::string &password, int port = 8000)
    {
        // ��ʼ��SDK
        if (!NET_DVR_Init())
        {
            std::cout << "SDK��ʼ��ʧ�ܣ������룺" << NET_DVR_GetLastError() << std::endl;
            return false;
        }
        std::cout << "SDK��ʼ���ɹ�" << std::endl;

        // ��������ʱ��������ʱ�� - �Ż�ʵʱ��
        NET_DVR_SetConnectTime(1000, 1);  // �������ӳ�ʱʱ��
        NET_DVR_SetReconnect(5000, true); // �����������

        // �����쳣�ص�
        NET_DVR_SetExceptionCallBack_V30(0, NULL, exceptionCallback, NULL);

        // ��¼�豸
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
            std::cout << "�豸��¼ʧ�ܣ������룺" << NET_DVR_GetLastError() << std::endl;
            NET_DVR_Cleanup();
            return false;
        }
        std::cout << "�豸��¼�ɹ����û�ID��" << m_userId << std::endl;

        // ��ʼ��˫ͨ�����ſ�
        if (!initPlayback())
        {
            NET_DVR_Logout(m_userId);
            NET_DVR_Cleanup();
            return false;
        }

        return true;
    }

    // ��ʼ��˫ͨ�����ſ�
    bool initPlayback()
    {
        for (int channel = 0; channel < 2; channel++)
        {
            // ��ȡ���Ŷ˿�
            if (!PlayM4_GetPort(&m_playPort[channel]))
            {
                std::cout << "ͨ��" << (channel + 1) << "��ȡ���Ŷ˿�ʧ��" << std::endl;
                return false;
            }
            std::cout << "ͨ��" << (channel + 1) << "��ȡ���Ŷ˿ڳɹ���" << m_playPort[channel] << std::endl;

            // ���˿���ͨ����Ϣ���� - ���ǹؼ�������ص������û�ָ������
            {
                std::lock_guard<std::mutex> lock(g_mapMutex);
                // �洢ָ��ǰ�����ָ���ͨ����Ϣ
                g_portMap[m_playPort[channel]] = reinterpret_cast<void *>((static_cast<uintptr_t>(reinterpret_cast<uintptr_t>(this)) << 8) | channel);
            }

            // ���ò���ģʽΪʵʱ��ģʽ - �Ż��ӳ�
            if (!PlayM4_SetStreamOpenMode(m_playPort[channel], STREAME_REALTIME))
            {
                std::cout << "ͨ��" << (channel + 1) << "������ģʽʧ��" << std::endl;
                return false;
            }

            // ���� - ���ٻ�������С�Խ����ӳ�
            if (!PlayM4_OpenStream(m_playPort[channel], nullptr, 0, 512 * 1024)) // ���ٻ�����
            {
                std::cout << "ͨ��" << (channel + 1) << "����ʧ��" << std::endl;
                return false;
            }

            // ���ý���ص�
            if (!PlayM4_SetDecCallBackExMend(m_playPort[channel], decodeCallback, 0, 0, 0))
            {
                std::cout << "ͨ��" << (channel + 1) << "���ý���ص�ʧ��" << std::endl;
                return false;
            }

            // ��ʼ����
            if (!PlayM4_Play(m_playPort[channel], nullptr))
            {
                std::cout << "ͨ��" << (channel + 1) << "��ʼ����ʧ��" << std::endl;
                return false;
            }

            std::cout << "ͨ��" << (channel + 1) << "���ſ��ʼ���ɹ�" << std::endl;
        }

        return true;
    }

    // ��ʼ˫ͨ��Ԥ��
    bool startPreview()
    {
        for (int channel = 0; channel < 2; channel++)
        {
            NET_DVR_PREVIEWINFO previewInfo;
            memset(&previewInfo, 0, sizeof(NET_DVR_PREVIEWINFO));

            previewInfo.hPlayWnd = nullptr;     // ����Ҫ���ھ���������ûص�
            previewInfo.lChannel = channel + 1; // ͨ���ţ�1��2��
            previewInfo.dwStreamType = 0;       // ������
            previewInfo.dwLinkMode = 0;         // TCPģʽ
            previewInfo.bBlocked = 0;           // ������ģʽ - ���ʵʱ��

            // ��ʼʵʱԤ����ע�����ݻص�
            m_playHandle[channel] = NET_DVR_RealPlay_V40(m_userId, &previewInfo, dataCallback,
                                                         reinterpret_cast<void *>((static_cast<uintptr_t>(reinterpret_cast<uintptr_t>(this)) << 8) | channel));

            if (m_playHandle[channel] < 0)
            {
                std::cout << "ͨ��" << (channel + 1) << "��ʼԤ��ʧ�ܣ������룺" << NET_DVR_GetLastError() << std::endl;
                return false;
            }

            std::cout << "ͨ��" << (channel + 1) << "��ʼԤ���ɹ������ž����" << m_playHandle[channel] << std::endl;
        }

        return true;
    }

    // ֹͣ˫ͨ��Ԥ��
    void stopPreview()
    {
        for (int channel = 0; channel < 2; channel++)
        {
            if (m_playHandle[channel] >= 0)
            {
                NET_DVR_StopRealPlay(m_playHandle[channel]);
                m_playHandle[channel] = -1;
                std::cout << "ͨ��" << (channel + 1) << "ֹͣԤ��" << std::endl;
            }
        }
    }

    // ������Դ
    void cleanup()
    {
        stopPreview();

        // ����˫ͨ�����ſ���Դ
        for (int channel = 0; channel < 2; channel++)
        {
            if (m_playPort[channel] >= 0)
            {
                PlayM4_Stop(m_playPort[channel]);
                PlayM4_CloseStream(m_playPort[channel]);
                PlayM4_FreePort(m_playPort[channel]);

                // ��ӳ�����Ƴ� - ��ֹҰָ��
                {
                    std::lock_guard<std::mutex> lock(g_mapMutex);
                    g_portMap.erase(m_playPort[channel]);
                }

                m_playPort[channel] = -1;
                std::cout << "ͨ��" << (channel + 1) << "�ͷŲ��Ŷ˿�" << std::endl;
            }
        }

        // �ǳ��豸
        if (m_userId >= 0)
        {
            NET_DVR_Logout(m_userId);
            m_userId = -1;
            std::cout << "�豸�ǳ�" << std::endl;
        }

        // ����SDK
        NET_DVR_Cleanup();
        std::cout << "SDK�������" << std::endl;
    }

private:
    LONG m_userId;        // �û���¼ID
    LONG m_playHandle[2]; // ˫ͨ��Ԥ�����
    LONG m_playPort[2];   // ˫ͨ�����Ŷ˿�

    // �쳣�ص�����
    static void CALLBACK exceptionCallback(DWORD dwType, LONG lUserID, LONG lHandle, void *pUser)
    {
        switch (dwType)
        {
        case EXCEPTION_RECONNECT:
            std::cout << "Ԥ��������ʱ�䣺" << time(NULL) << std::endl;
            break;
        default:
            std::cout << "�쳣���ͣ�" << dwType << std::endl;
            break;
        }
    }

    // ʵʱ���ݻص�����
    static void CALLBACK dataCallback(LONG lPlayHandle, DWORD dwDataType,
                                      BYTE *pBuffer, DWORD dwBufSize, void *pUser)
    {
        // �����û����ݻ�ȡ����ָ���ͨ����Ϣ
        uintptr_t userValue = reinterpret_cast<uintptr_t>(pUser);
        HikCameraCapture *camera = reinterpret_cast<HikCameraCapture *>(userValue >> 8);
        int channel = static_cast<int>(userValue & 0xFF);

        // ֻ������Ƶ������
        if (dwDataType != NET_DVR_STREAMDATA || !camera || !pBuffer || dwBufSize == 0 || channel > 1)
        {
            return;
        }

        // ���������벥�ſ���н���
        if (camera->m_playPort[channel] >= 0)
        {
            PlayM4_InputData(camera->m_playPort[channel], pBuffer, dwBufSize);
        }
    }

    // ����ص���������YUV����ת��ΪBGR��ʽ��Mat
    static void CALLBACK decodeCallback(long nPort, char *pBuf, long nSize,
                                        FRAME_INFO *pFrameInfo, long nUser, long nReserved2)
    {
        // ͨ���˿ڲ��Ҷ�Ӧ������ͷ�����ͨ��
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

        // ����YV12��ʽ��YUV����
        if (pFrameInfo->nType == T_YV12)
        {
            try
            {
                // ����YUV Mat
                cv::Mat yuvMat(pFrameInfo->nHeight + pFrameInfo->nHeight / 2,
                               pFrameInfo->nWidth, CV_8UC1, (uchar *)pBuf);

                // ת��ΪBGR��ʽ
                cv::Mat bgrMat;
                cv::cvtColor(yuvMat, bgrMat, cv::COLOR_YUV2BGR_YV12);

                // ����ͨ�����¶�Ӧ��ȫ��֡����
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

                // ÿ60֡����һ��֡��
                static int totalFrames = 0;
                if (++totalFrames % 60 == 0)
                {
                    auto currentTime = std::chrono::steady_clock::now();
                    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - g_lastTime);
                    if (duration.count() > 0)
                    {
                        g_fps1 = (g_frameCount1.load() * 1000.0) / duration.count();
                        g_fps2 = (g_frameCount2.load() * 1000.0) / duration.count();

                        // ���ü�����
                        g_frameCount1 = 0;
                        g_frameCount2 = 0;
                        g_lastTime = currentTime;
                    }
                }
            }
            catch (const cv::Exception &e)
            {
                std::cout << "OpenCV�쳣��" << e.what() << std::endl;
            }
            catch (...)
            {
                std::cout << "����ص��쳣" << std::endl;
            }
        }
    }
};

// ˫ͨ����ʾ�̺߳���
void displayThread()
{
    // ������������
    cv::namedWindow("Channel 1 - Hikvision Camera", cv::WINDOW_AUTOSIZE);
    cv::namedWindow("Channel 2 - Hikvision Camera", cv::WINDOW_AUTOSIZE);

    // ���ô���λ��
    cv::moveWindow("Channel 1 - Hikvision Camera", 100, 100);
    cv::moveWindow("Channel 2 - Hikvision Camera", 800, 100);

    while (g_running.load())
    {
        cv::Mat frame1, frame2;

        // ��ȡ����ͨ����֡����
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

        // ��ʾͨ��1
        if (!frame1.empty())
        {
            // ��ͼ���ϻ���FPS��Ϣ
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

        // ��ʾͨ��2
        if (!frame2.empty())
        {
            // ��ͼ���ϻ���FPS��Ϣ
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

        // ��ESC�˳� - �����ӳ�
        char key = cv::waitKey(1) & 0xFF; // ��Ϊ1ms�����ӳ�
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
    std::cout << "=== ��������˫ͨ������ͷ + OpenCV ��ʾ���� ===" << std::endl;

    // ��������ͷ����
    g_camera = new HikCameraCapture();

    // ��ʼ������ͷ�����޸�Ϊ��������ͷ������
    std::string ip = "192.168.1.64";       // ����ͷIP��ַ
    std::string username = "admin";        // �û���
    std::string password = "tkytjsyjs111"; // ����
    int port = 8553;                       // �˿�

    std::cout << "������������ͷ " << ip << ":" << port << std::endl;

    if (!g_camera->initialize(ip, username, password, port))
    {
        std::cout << "����ͷ��ʼ��ʧ�ܣ�" << std::endl;
        delete g_camera;
        return -1;
    }

    std::cout << "����ͷ��ʼ���ɹ�����ʼ˫ͨ��Ԥ��..." << std::endl;

    if (!g_camera->startPreview())
    {
        std::cout << "��ʼԤ��ʧ�ܣ�" << std::endl;
        delete g_camera;
        return -1;
    }

    std::cout << "˫ͨ��Ԥ���ѿ�ʼ��������ʾ����..." << std::endl;
    std::cout << "��ESC���˳�����" << std::endl;

    // ������ʾ�߳�
    std::thread display(displayThread);

    // ���̵߳ȴ�
    display.join();

    std::cout << "�����˳���������Դ..." << std::endl;

    // ������Դ
    g_running.store(false);
    delete g_camera;
    g_camera = nullptr;

    std::cout << "�������" << std::endl;
    return 0;
}
