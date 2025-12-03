#include <fstream>
#include "ThreadManager.h"
#include "SharedData.h"
#include "ObjectTrackingConfig.h" // 包含目标追踪配置头文件
#include <opencv2/opencv.hpp>
#include <iostream>
#include <cstdlib>
#include <nlohmann/json.hpp>
#include <mutex>
#include <atomic>
#include "ControlServer.h"

using json = nlohmann::json;
bool loadConfig(json &config);
std::vector<std::string> generateStreamUrls(const json &config);

int main()
{
	// 加载配置文件
	json config;
	if (!loadConfig(config))
	{
		std::cerr << "[Main] 配置文件加载失败" << std::endl;
		return -1;
	}

	// 启动 rtsp-simple-server.exe
	std::string cmd = "start \"\" \"" +
					  config["rtsp_server"]["exe_path"].get<std::string>() + "\" \"" +
					  config["rtsp_server"]["config_path"].get<std::string>() + "\"";
	std::cout << "[Main] 启动RTSP服务器: " << cmd << std::endl;
	system(cmd.c_str());
	std::cout << "[Main] RTSP服务器已启动" << std::endl;

	// 获取摄像头数量配置
	int cameraCount = config["camera_count"].get<int>();
	std::cout << "[Main] 摄像头数量配置: " << cameraCount << std::endl;

	if (cameraCount < 1 || cameraCount > 2)
	{
		std::cerr << "[Main] 无效的摄像头数量配置，必须为1或2" << std::endl;
		return -1;
	}

	// 获取设备配置 - 改进的逻辑，支持灵活的设备配置
	std::vector<json> deviceConfigs;
	std::vector<json> availableDevices = config["hikvision_devices"];

	std::cout << "[Main] 可用设备配置数量: " << availableDevices.size() << std::endl;

	// 如果摄像头数量为1，尝试找到第一个可用的设备
	if (cameraCount == 1)
	{
		bool deviceFound = false;
		for (size_t i = 0; i < availableDevices.size(); i++)
		{
			const auto &device = availableDevices[i];
			std::cout << "[Main] 检查设备配置" << (i + 1) << ": "
					  << device["name"].get<std::string>() << " ("
					  << device["ip"].get<std::string>() << ":"
					  << device["port"].get<int>() << ")" << std::endl;

			// 对于单摄像头场景，直接使用第一个非空IP的配置
			std::string ip = device["ip"].get<std::string>();
			if (!ip.empty() && ip != "0.0.0.0")
			{
				deviceConfigs.push_back(device);
				std::cout << "[Main] 选择使用设备配置" << (i + 1) << ": " << device["name"].get<std::string>() << std::endl;
				deviceFound = true;
				break;
			}
		}

		// 如果没找到合适的设备，使用第一个配置（兼容原有逻辑）
		if (!deviceFound && !availableDevices.empty())
		{
			deviceConfigs.push_back(availableDevices[0]);
			std::cout << "[Main] 未找到明确的设备配置，使用第一个配置" << std::endl;
		}
	}
	else
	{
		// 多摄像头场景，按顺序使用配置
		for (int i = 0; i < cameraCount; i++)
		{
			if (i < availableDevices.size())
			{
				deviceConfigs.push_back(availableDevices[i]);
			}
			else
			{
				std::cerr << "[Main] 设备配置不足，需要 " << cameraCount << " 个设备配置" << std::endl;
				return -1;
			}
		}
	}

	if (deviceConfigs.empty())
	{
		std::cerr << "[Main] 没有找到可用的设备配置" << std::endl;
		return -1;
	}

	std::cout << "[Main] 最终使用的设备配置数量: " << deviceConfigs.size() << std::endl;

	// 生成推流地址
	std::vector<std::string> rtspUrls = generateStreamUrls(config);

	// 从配置文件加载目标追踪参数
	ObjectTrackingConfig trackingConfig;
	if (!trackingConfig.loadFromJson(config))
	{
		std::cerr << "[Main] 目标追踪配置加载失败，使用默认参数" << std::endl;
	}

	// 验证配置参数有效性
	if (!trackingConfig.isValid())
	{
		std::cerr << "[Main] 目标追踪配置参数无效！" << std::endl;
		return -1;
	}

	// 初始化共享数据对象
	SharedData sharedData;
	sharedData.isRunning = true;
	// 移除测试模式，直接使用生产模式
	// sharedData.isTestMode = false;

	// 加载视频保存配置（基于海康SDK）
	if (config.contains("video_save"))
	{
		const auto &videoSaveConfig = config["video_save"];
		sharedData.videoSaveConfig.enableVideoSave = videoSaveConfig.value("enable_video_save", false);
		sharedData.videoSaveConfig.videoSavePath = videoSaveConfig.value("video_save_path", "D:/RailwayVideos/");
		sharedData.videoSaveConfig.maxFileSizeMB = videoSaveConfig.value("max_file_size_mb", 1024);
		sharedData.videoSaveConfig.maxStorageGB = videoSaveConfig.value("max_storage_gb", 600);
		sharedData.videoSaveConfig.cleanupSizeGB = videoSaveConfig.value("cleanup_size_gb", 40);

		std::cout << "[Main] Video save configuration loaded (Hikvision SDK mode):" << std::endl;
		std::cout << "  - Enabled: " << (sharedData.videoSaveConfig.enableVideoSave ? "Yes" : "No") << std::endl;
		std::cout << "  - Save path: " << sharedData.videoSaveConfig.videoSavePath << std::endl;
		std::cout << "  - Max file size: " << sharedData.videoSaveConfig.maxFileSizeMB << "MB (SDK auto-split)" << std::endl;
		std::cout << "  - Max storage: " << sharedData.videoSaveConfig.maxStorageGB << "GB" << std::endl;
		std::cout << "  - Cleanup size: " << sharedData.videoSaveConfig.cleanupSizeGB << "GB" << std::endl;
	}
	else
	{
		std::cout << "[Main] Video save configuration not found, using default settings (disabled)" << std::endl;
	}

	// 加载热成像处理配置
	if (config.contains("thermal_processing"))
	{
		const auto &thermalConfig = config["thermal_processing"];
		sharedData.thermalProcessingConfig.enableThermalProcessing = thermalConfig.value("enable_thermal_processing", true);
		sharedData.thermalProcessingConfig.environmentTempThreshold = thermalConfig.value("environment_temp_threshold", 50.0f);

		std::cout << "[Main] Thermal processing configuration loaded:" << std::endl;
		std::cout << "  - Enabled: " << (sharedData.thermalProcessingConfig.enableThermalProcessing ? "Yes" : "No") << std::endl;
		std::cout << "  - Environment temp threshold: " << sharedData.thermalProcessingConfig.environmentTempThreshold << "°C" << std::endl;
	}
	else
	{
		std::cout << "[Main] Thermal processing configuration not found, using default settings" << std::endl;
	}

	std::cout << "[Main] 系统运行在生产模式，摄像头数量: " << cameraCount << std::endl;

	// 启动控制服务器（独立文本协议，用于端点切换）
	ControlServer controlServer;
	uint16_t controlPort = 12347; // 默认端口；如需改为从配置读取，可在此处解析
	if (!controlServer.start(controlPort))
	{
		std::cerr << "[Main] ControlServer start failed on port " << controlPort << std::endl;
	}

	// 读取RTSP推流配置参数
	int streamWidth = 0, streamHeight = 0, streamFps = 25;
	if (config.contains("rtsp_streaming"))
	{
		auto &rtspConfig = config["rtsp_streaming"];
		if (rtspConfig.contains("resolution"))
		{
			streamWidth = rtspConfig["resolution"]["width"].get<int>();
			streamHeight = rtspConfig["resolution"]["height"].get<int>();
		}
		if (rtspConfig.contains("fps"))
		{
			streamFps = rtspConfig["fps"].get<int>();
		}
		std::cout << "[Main] RTSP推流配置 - 分辨率: " << streamWidth << "x" << streamHeight
				  << " (0表示使用原始分辨率), 帧率: " << streamFps << std::endl;
	}

	// 创建线程管理器（SDK登录逻辑已移至TaskVideoCapture）
	ThreadManager manager(cameraCount, deviceConfigs, sharedData, rtspUrls, trackingConfig, streamWidth, streamHeight, streamFps);

	// 启动所有线程
	std::cout << "[Main] 启动所有任务线程..." << std::endl;
	manager.startAll();

	// 主线程循环，等待线程退出
	std::cout << "[Main] 系统启动完成，按Ctrl+C退出程序" << std::endl;
	while (sharedData.isRunning)
	{
		std::this_thread::sleep_for(std::chrono::seconds(1));
	}

	// 停止所有线程
	std::cout << "[Main] 开始停止所有任务线程..." << std::endl;
	sharedData.isRunning = false; // 确保在停止线程前设置标志
	manager.stopAll();

	// 停止控制服务器
	controlServer.stop();

	// 清理资源
	rtspUrls.clear();													  // 清空推流地址列表
	cv::destroyAllWindows();											  // 关闭所有OpenCV窗口
	std::system("taskkill /FI \"WINDOWTITLE eq rtsp-simple-server\" /F"); // 关闭rtsp-simple-server.exe

	std::cout << "[Main] 程序已正常退出" << std::endl;
	return 0;
}

bool loadConfig(json &config)
{
	std::ifstream f("config.json");
	if (!f.is_open())
	{
		std::cerr << "[Main] 无法打开配置文件config.json" << std::endl;
		return false;
	}
	try
	{
		config = json::parse(f);
		return true;
	}
	catch (const json::exception &e)
	{
		std::cerr << "[Main] 配置文件解析错误: " << e.what() << std::endl;
		return false;
	}
}

std::vector<std::string> generateStreamUrls(const json &config)
{
	std::vector<std::string> urls;
	std::string ip1 = config["stream_urls"]["local_ip1"];
	std::string ip2 = config["stream_urls"]["local_ip2"];

	// 从配置文件获取RTSP端口
	int port = config["stream_urls"]["rtsp_port"].get<int>();

	// 生成推流地址：热成像1、可见光1、热成像2、可见光2
	urls.push_back("rtsp://" + ip1 + ":" + std::to_string(port) + "/thermal1"); // 设备1热成像
	urls.push_back("rtsp://" + ip1 + ":" + std::to_string(port) + "/visible1"); // 设备1可见光
	urls.push_back("rtsp://" + ip2 + ":" + std::to_string(port) + "/thermal2"); // 设备2热成像
	urls.push_back("rtsp://" + ip2 + ":" + std::to_string(port) + "/visible2"); // 设备2可见光

	return urls;
}

// urls[0] = "rtsp://127.0.0.1:8556/thermal1"     // 设备1热成像
// urls[1] = "rtsp://127.0.0.1:8556/visible1"     // 设备1可见光
// urls[2] = "rtsp://127.0.0.1:8556/thermal2" // 设备2热成像
// urls[3] = "rtsp://127.0.0.1:8556/visible2" // 设备2可见光
