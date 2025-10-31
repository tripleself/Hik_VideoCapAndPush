#include "TaskVideoCapture.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <thread>
#include <chrono>
#include <vector>
#include <nlohmann/json.hpp>
#include <fstream>

// 海康威视时间戳解析宏定义
#define GET_YEAR(_time_) (((_time_) >> 26) + 2000)
#define GET_MONTH(_time_) (((_time_) >> 22) & 15)
#define GET_DAY(_time_) (((_time_) >> 17) & 31)
#define GET_HOUR(_time_) (((_time_) >> 12) & 31)
#define GET_MINUTE(_time_) (((_time_) >> 6) & 63)
#define GET_SECOND(_time_) (((_time_) >> 0) & 63)

// 静态成员初始化
std::map<LONG, std::pair<int, int>> TaskVideoCapture::portMap_;
std::mutex TaskVideoCapture::portMapMutex_;
TaskVideoCapture *TaskVideoCapture::instance_ = nullptr;
std::mutex TaskVideoCapture::instanceMutex_;

/**
 * @brief TaskVideoCapture构造函数
 * 初始化海康威视SDK视频捕获架构
 */
TaskVideoCapture::TaskVideoCapture(int cameraCount, const std::vector<nlohmann::json> &deviceConfigs, SharedData &data)
	: cameraCount_(cameraCount), deviceConfigs_(deviceConfigs), data_(data)
{
	std::cout << "[TaskVideoCapture] 初始化海康SDK视频捕获，摄像头数量: " << cameraCount_ << std::endl;

	// 设置静态实例指针
	{
		std::lock_guard<std::mutex> lock(instanceMutex_);
		instance_ = this;
	}

	// 初始化数据结构
	userIDs_.resize(cameraCount_, -1);
	deviceLoginSuccess_.resize(cameraCount_, false); // 初始化登录状态为失败
	playHandles_.resize(cameraCount_);
	playPorts_.resize(cameraCount_);
	frameBuffers_.resize(cameraCount_);
	frameMutexes_.resize(cameraCount_);

	// 初始化测温相关数据结构
	thermometryHandles_.resize(cameraCount_, -1);
	thermometryActive_.resize(cameraCount_, false);

	// 初始化SDK视频保存相关数据结构
	videoSaveActive_.resize(cameraCount_, false);
	videoSaveThreads_.resize(cameraCount_);
	videoSaveHandles_.resize(cameraCount_, -1);
	shouldStopVideoSave_ = false;

	for (int i = 0; i < cameraCount_; i++)
	{
		playHandles_[i].fill(-1);
		playPorts_[i].fill(-1);

		// 初始化mutex指针
		for (int j = 0; j < 2; j++)
		{
			frameMutexes_[i][j] = std::make_unique<std::mutex>();
		}
	}

	std::cout << "[TaskVideoCapture] SDK视频保存功能已初始化" << std::endl;
}

/**
 * @brief TaskVideoCapture析构函数
 * 确保线程安全退出和资源清理
 */
TaskVideoCapture::~TaskVideoCapture()
{
	stop();

	// 停止SDK视频保存
	shouldStopVideoSave_ = true;
	for (int i = 0; i < cameraCount_; ++i)
	{
		stopSDKVideoSave(i);
	}

	// 停止存储监控线程
	if (storageMonitorThread_.joinable())
	{
		storageMonitorThread_.join();
	}

	std::cout << "[TaskVideoCapture] SDK视频保存已停止" << std::endl;

	cleanup();

	// 清除静态实例指针
	{
		std::lock_guard<std::mutex> lock(instanceMutex_);
		if (instance_ == this)
		{
			instance_ = nullptr;
		}
	}
}

/**
 * @brief 启动视频捕获线程
 */
void TaskVideoCapture::start()
{
	std::cout << "[TaskVideoCapture] 启动海康SDK视频捕获线程..." << std::endl;
	thread_ = std::thread(&TaskVideoCapture::run, this);
}

/**
 * @brief 停止视频捕获线程
 */
void TaskVideoCapture::stop()
{
	std::cout << "[TaskVideoCapture] 停止海康SDK视频捕获线程..." << std::endl;
	data_.isRunning = false;

	if (thread_.joinable())
	{
		thread_.join();
	}
	std::cout << "[TaskVideoCapture] 海康SDK视频捕获线程已安全停止" << std::endl;
}

/**
 * @brief 初始化海康SDK
 */
bool TaskVideoCapture::initializeSDK()
{
	// 初始化SDK
	if (!NET_DVR_Init())
	{
		std::cerr << "[TaskVideoCapture] SDK初始化失败，错误码: " << NET_DVR_GetLastError() << std::endl;
		return false;
	}
	std::cout << "[TaskVideoCapture] SDK初始化成功" << std::endl;

	// 设置连接时间与重连时间 - 参考VideoCapSDK的设置
	NET_DVR_SetConnectTime(2000, 1);   // 连接超时2秒，尝试1次
	NET_DVR_SetReconnect(10000, true); // 重连间隔10秒，开启重连

	// 设置异常回调
	NET_DVR_SetExceptionCallBack_V30(0, NULL, exceptionCallback, NULL);

	std::cout << "[TaskVideoCapture] SDK配置完成：连接超时2秒，重连间隔10秒" << std::endl;

	return true;
}

/**
 * @brief 登录设备
 */
bool TaskVideoCapture::loginDevices()
{
	std::cout << "[TaskVideoCapture] 开始登录设备，摄像头数量: " << cameraCount_ << "，配置项数量: " << deviceConfigs_.size() << std::endl;

	// 检查配置项数量是否足够
	if (deviceConfigs_.size() < static_cast<size_t>(cameraCount_))
	{
		std::cerr << "[TaskVideoCapture] 错误：配置项数量(" << deviceConfigs_.size() << ")小于摄像头数量(" << cameraCount_ << ")" << std::endl;
		return false;
	}

	int successCount = 0; // 成功登录的设备数量

	// 读取登录重试配置（可选）
	int maxRetries = 1;			// 默认最大重试次数
	int retryIntervalMs = 3000; // 默认重试间隔（毫秒）
	try
	{
		std::ifstream f("config.json");
		if (f.is_open())
		{
			nlohmann::json cfg;
			f >> cfg;
			f.close();

			if (cfg.contains("object_tracking") && cfg["object_tracking"].contains("device_login"))
			{
				const auto &dl = cfg["object_tracking"]["device_login"];
				if (dl.contains("max_retries"))
				{
					maxRetries = (std::max)(1, dl["max_retries"].get<int>());
				}
				if (dl.contains("retry_interval_ms"))
				{
					retryIntervalMs = (std::max)(0, dl["retry_interval_ms"].get<int>());
				}
			}
		}
		else
		{
			std::cout << "[TaskVideoCapture] 警告: 无法打开config.json，使用默认登录重试参数 maxRetries="
					  << maxRetries << ", retryIntervalMs=" << retryIntervalMs << std::endl;
		}
	}
	catch (const std::exception &e)
	{
		std::cout << "[TaskVideoCapture] 读取登录重试配置失败，使用默认值。原因: " << e.what() << std::endl;
	}

	// 固定双摄像头模式：设备1=一位端，设备2=二位端（按设备串行重试登录）
	for (int i = 0; i < cameraCount_; i++)
	{
		const auto &deviceConfig = deviceConfigs_[i];
		std::string deviceName = (i == 0) ? "一位端(设备1)" : "二位端(设备2)";

		// 显示当前尝试登录的设备信息
		std::cout << "[TaskVideoCapture] 准备登录 " << deviceName << ": "
				  << deviceConfig["name"].get<std::string>() << " - "
				  << deviceConfig["ip"].get<std::string>() << ":"
				  << deviceConfig["port"].get<int>() << ", 最大重试: " << maxRetries
				  << ", 间隔(ms): " << retryIntervalMs << std::endl;

		// 初始化登录状态
		userIDs_[i] = -1;
		deviceLoginSuccess_[i] = false;

		// 组装登录信息（每次重试可复用）
		NET_DVR_USER_LOGIN_INFO loginInfo = {0};
		loginInfo.bUseAsynLogin = 0;
		strncpy_s(loginInfo.sDeviceAddress, deviceConfig["ip"].get<std::string>().c_str(), NET_DVR_DEV_ADDRESS_MAX_LEN);
		strncpy_s(loginInfo.sUserName, deviceConfig["username"].get<std::string>().c_str(), NAME_LEN);
		strncpy_s(loginInfo.sPassword, deviceConfig["password"].get<std::string>().c_str(), PASSWD_LEN);
		loginInfo.wPort = static_cast<WORD>(deviceConfig["port"].get<int>());

		NET_DVR_DEVICEINFO_V40 deviceInfo;
		memset(&deviceInfo, 0, sizeof(NET_DVR_DEVICEINFO_V40));

		for (int attempt = 1; attempt <= maxRetries; ++attempt)
		{
			userIDs_[i] = NET_DVR_Login_V40(&loginInfo, &deviceInfo);
			if (userIDs_[i] >= 0)
			{
				successCount++;
				deviceLoginSuccess_[i] = true;
				std::cout << "[TaskVideoCapture] " << deviceName << " 登录成功(第" << attempt << "次)，用户ID: "
						  << userIDs_[i] << " (IP: " << deviceConfig["ip"].get<std::string>() << ":"
						  << deviceConfig["port"].get<int>() << ")" << std::endl;
				break; // 当前设备成功后继续下一个设备
			}
			else
			{
				DWORD errorCode = NET_DVR_GetLastError();
				int remaining = maxRetries - attempt;
				std::cerr << "[TaskVideoCapture] " << deviceName << " 登录失败(第" << attempt << "次)，错误码: "
						  << errorCode << ", 剩余重试次数: " << remaining
						  << " (IP: " << deviceConfig["ip"].get<std::string>() << ":"
						  << deviceConfig["port"].get<int>() << ")" << std::endl;

				userIDs_[i] = -1;
				deviceLoginSuccess_[i] = false;

				if (remaining > 0 && retryIntervalMs > 0)
				{
					std::this_thread::sleep_for(std::chrono::milliseconds(retryIntervalMs));
				}
			}
		}
	}

	std::cout << "[TaskVideoCapture] 设备登录完成，成功登录 " << successCount << "/" << cameraCount_ << " 个设备" << std::endl;

	// 只要至少有一个设备登录成功，程序就继续执行
	if (successCount > 0)
	{
		std::cout << "[TaskVideoCapture] 至少有一个设备登录成功，程序继续执行" << std::endl;
		return true;
	}
	else
	{
		std::cerr << "[TaskVideoCapture] 所有设备登录失败，程序退出" << std::endl;
		return false;
	}
}

// TaskVideoCapture.cpp 中添加实现
/**
 * @brief 配置指定设备的热成像参数
 * @param deviceIdx 设备索引
 * @return true 如果配置成功，否则 false
 */
bool TaskVideoCapture::configureThermometry(int deviceIdx)
{
	std::cout << "[TaskVideoCapture] 开始为设备 " << (deviceIdx + 1) << " 配置热成像参数..." << std::endl;

	LONG userID = userIDs_[deviceIdx];

	// 首先检查设备能力集，验证是否支持热成像配置
	std::cout << "[TaskVideoCapture] 检查设备 " << (deviceIdx + 1) << " 热成像能力集..." << std::endl;

	// 尝试使用简单的能力检查 - 直接尝试获取热成像配置来判断支持性
	NET_DVR_THERMOMETRY_BASICPARAM testParams;
	memset(&testParams, 0, sizeof(testParams));
	testParams.dwSize = sizeof(NET_DVR_THERMOMETRY_BASICPARAM);
	DWORD testReturnedSize = 0;

	// 构造通道号条件参数
	LONG channelNo = 1;

	// 构造标准配置结构体
	NET_DVR_STD_CONFIG stdConfig;
	memset(&stdConfig, 0, sizeof(stdConfig));
	stdConfig.lpCondBuffer = &channelNo;	  // 通道号作为条件参数
	stdConfig.dwCondSize = sizeof(LONG);	  // 条件参数大小
	stdConfig.lpInBuffer = NULL;			  // 获取时输入为空
	stdConfig.dwInSize = 0;					  // 输入大小为0
	stdConfig.lpOutBuffer = &testParams;	  // 输出缓冲区
	stdConfig.dwOutSize = sizeof(testParams); // 输出缓冲区大小
	stdConfig.lpStatusBuffer = NULL;		  // 状态缓冲区
	stdConfig.dwStatusSize = 0;				  // 状态缓冲区大小
	stdConfig.lpXmlBuffer = NULL;			  // XML缓冲区
	stdConfig.dwXmlSize = 0;				  // XML大小
	stdConfig.byDataType = 0;				  // 使用结构体类型

	// 先用通道1测试是否支持热成像功能
	if (!NET_DVR_GetSTDConfig(userID, NET_DVR_GET_THERMOMETRY_BASICPARAM, &stdConfig))
	{
		DWORD errorCode = NET_DVR_GetLastError();
		if (errorCode == 26) // 不支持该功能
		{
			std::cout << "[TaskVideoCapture] 设备 " << (deviceIdx + 1) << " 不支持热成像功能，错误码: " << errorCode << std::endl;
			return true; // 不支持则跳过，继续其他设备
		}
		std::cout << "[TaskVideoCapture] 设备 " << (deviceIdx + 1) << " 通道1热成像能力检查失败, 错误码: " << errorCode << "，将继续尝试其他通道" << std::endl;
	}
	else
	{
		// 从返回的缓冲区读取数据
		DWORD bytesReturned = stdConfig.dwOutSize;
		std::cout << "[TaskVideoCapture] 设备 " << (deviceIdx + 1) << " 支持热成像配置，返回数据大小: " << bytesReturned << std::endl;
		std::cout << "[TaskVideoCapture] 当前测温使能状态: " << (int)testParams.byEnabled << std::endl;
	}

	// 热成像通道号为2
	std::vector<LONG> channels = {2}; // 简化通道列表，先尝试常用的

	for (LONG channel : channels)
	{
		std::cout << "[TaskVideoCapture] 尝试通道 " << channel << "..." << std::endl;

		NET_DVR_THERMOMETRY_BASICPARAM thermometryParams;
		memset(&thermometryParams, 0, sizeof(thermometryParams)); // 完全清零

		// 构造获取配置的标准结构体
		NET_DVR_STD_CONFIG getConfig;
		memset(&getConfig, 0, sizeof(getConfig));
		getConfig.lpCondBuffer = &channel;				 // 通道号作为条件参数
		getConfig.dwCondSize = sizeof(LONG);			 // 条件参数大小
		getConfig.lpInBuffer = NULL;					 // 获取时输入为空
		getConfig.dwInSize = 0;							 // 输入大小为0
		getConfig.lpOutBuffer = &thermometryParams;		 // 输出缓冲区
		getConfig.dwOutSize = sizeof(thermometryParams); // 输出缓冲区大小
		getConfig.lpStatusBuffer = NULL;				 // 状态缓冲区
		getConfig.dwStatusSize = 0;						 // 状态缓冲区大小
		getConfig.lpXmlBuffer = NULL;					 // XML缓冲区
		getConfig.dwXmlSize = 0;						 // XML大小
		getConfig.byDataType = 0;						 // 使用结构体类型

		// 获取当前配置作为基础
		bool hasCurrentConfig = NET_DVR_GetSTDConfig(userID, NET_DVR_GET_THERMOMETRY_BASICPARAM, &getConfig);
		if (!hasCurrentConfig)
		{
			memset(&thermometryParams, 0, sizeof(thermometryParams));
		}

		// 直接设置想要的参数
		thermometryParams.dwSize = sizeof(NET_DVR_THERMOMETRY_BASICPARAM);
		thermometryParams.byEnabled = 1;
		thermometryParams.byShowTempStripEnable = 1;
		thermometryParams.byThermometryUnit = 0; // 摄氏度
		thermometryParams.byThermometryRange = 2;

		// 设置配置
		NET_DVR_STD_CONFIG setConfig;
		memset(&setConfig, 0, sizeof(setConfig));
		setConfig.lpCondBuffer = &channel;
		setConfig.dwCondSize = sizeof(LONG);
		setConfig.lpInBuffer = &thermometryParams;
		setConfig.dwInSize = sizeof(thermometryParams);
		setConfig.byDataType = 0;

		if (NET_DVR_SetSTDConfig(userID, NET_DVR_SET_THERMOMETRY_BASICPARAM, &setConfig))
		{
			std::cout << "[TaskVideoCapture] 设备 " << (deviceIdx + 1) << " 通道 " << channel << " 热成像基础配置成功" << std::endl;

			// 配置热成像前端参数（AGC和调色板）
			configureThermalCameraParams(deviceIdx, channel);

			return true;
		}
		else
		{
			std::cout << "[TaskVideoCapture] 设备 " << (deviceIdx + 1) << " 通道 " << channel << " 热成像基础配置失败" << std::endl;
			DWORD errorCode = NET_DVR_GetLastError();
			std::cout << "[TaskVideoCapture] 错误码: " << errorCode << std::endl;
		}

		// 如果完整配置失败，尝试简化配置（只设置温度条）
		if (hasCurrentConfig && NET_DVR_GetSTDConfig(userID, NET_DVR_GET_THERMOMETRY_BASICPARAM, &getConfig))
		{
			thermometryParams.dwSize = sizeof(NET_DVR_THERMOMETRY_BASICPARAM);
			thermometryParams.byShowTempStripEnable = 1;
			thermometryParams.byThermometryUnit = 0; // 摄氏度
			thermometryParams.byThermometryRange = 2;

			if (NET_DVR_SetSTDConfig(userID, NET_DVR_SET_THERMOMETRY_BASICPARAM, &setConfig))
			{
				std::cout << "[TaskVideoCapture] 设备 " << (deviceIdx + 1) << " 通道 " << channel << " 简化配置成功" << std::endl;

				// 配置热成像前端参数（AGC和调色板）
				configureThermalCameraParams(deviceIdx, channel);

				return true;
			}
			else
			{
				std::cout << "[TaskVideoCapture] 设备 " << (deviceIdx + 1) << " 通道 " << channel << " 简化配置失败" << std::endl;
				DWORD errorCode = NET_DVR_GetLastError();
				std::cout << "[TaskVideoCapture] 错误码: " << errorCode << std::endl;
			}
		}
	}
	return true; // 继续其他设备
}

/**
 * @brief 配置热成像前端参数（AGC和调色板）
 * @param deviceIdx 设备索引
 * @param channel 通道号
 * @return true 如果配置成功，否则 false
 */
bool TaskVideoCapture::configureThermalCameraParams(int deviceIdx, LONG channel)
{
	std::cout << "[TaskVideoCapture] 开始为设备 " << (deviceIdx + 1) << " 通道 " << channel << " 配置前端参数..." << std::endl;

	LONG userID = userIDs_[deviceIdx];

	NET_DVR_CAMERAPARAMCFG_EX cameraParams;
	memset(&cameraParams, 0, sizeof(cameraParams));

	// 获取当前前端参数配置
	DWORD returnedSize = 0;
	bool hasCurrentConfig = NET_DVR_GetDVRConfig(userID, NET_DVR_GET_CCDPARAMCFG_EX, channel,
												 &cameraParams, sizeof(cameraParams), &returnedSize);
	if (!hasCurrentConfig)
	{
		memset(&cameraParams, 0, sizeof(cameraParams));
	}

	// 设置基础参数
	cameraParams.dwSize = sizeof(NET_DVR_CAMERAPARAMCFG_EX);

	cameraParams.byDimmerMode = 0;
	// 设置调色板模式：1-黑热
	// cameraParams.byPaletteMode = 10;
	cameraParams.byEnhancedMode = 0;

	// 配置测温AGC参数
	cameraParams.struThermAGC.byMode = 2;			  // 自动AGC模式
	cameraParams.struThermAGC.iHighTemperature = 100; // 最高温度100°C
	cameraParams.struThermAGC.iLowTemperature = -20;  // 最低温度-20°C
	// 设置前端参数
	if (NET_DVR_SetDVRConfig(userID, NET_DVR_SET_CCDPARAMCFG_EX, channel,
							 &cameraParams, sizeof(cameraParams)))
	{
		std::cout << "[TaskVideoCapture] 设备 " << (deviceIdx + 1) << " 通道 " << channel << " 前端参数配置成功" << std::endl;
		return true;
	}
	else
	{
		DWORD errorCode = NET_DVR_GetLastError();
		std::cout << "[TaskVideoCapture] 设备 " << (deviceIdx + 1) << " 通道 " << channel
				  << " 前端参数配置失败，错误码: " << errorCode << std::endl;
		return false;
	}
}

/**
 * @brief 初始化播放库
 */
bool TaskVideoCapture::initializePlayback()
{
	for (int deviceIdx = 0; deviceIdx < cameraCount_; deviceIdx++)
	{
		// 只对成功登录的设备进行初始化
		if (!deviceLoginSuccess_[deviceIdx])
		{
			std::cout << "[TaskVideoCapture] 跳过设备" << (deviceIdx + 1) << "播放库初始化（设备未登录成功）" << std::endl;
			continue;
		}

		for (int channelIdx = 0; channelIdx < 2; channelIdx++) // 双通道：热成像+可见光
		{
			// 获取播放端口
			if (!PlayM4_GetPort(&playPorts_[deviceIdx][channelIdx]))
			{
				std::cerr << "[TaskVideoCapture] 设备" << (deviceIdx + 1) << "通道" << (channelIdx + 1) << "获取播放端口失败" << std::endl;
				return false;
			}
			std::cout << "[TaskVideoCapture] 设备" << (deviceIdx + 1) << "通道" << (channelIdx + 1) << "获取播放端口成功: " << playPorts_[deviceIdx][channelIdx] << std::endl;

			// 建立端口映射（参考VideoCapSDK的方式）
			{
				std::lock_guard<std::mutex> lock(portMapMutex_);
				// 使用与参考代码相同的映射方式：存储对象指针和通道信息
				uintptr_t thisPtr = reinterpret_cast<uintptr_t>(this);
				portMap_[playPorts_[deviceIdx][channelIdx]] = std::make_pair(deviceIdx, channelIdx);
			}

			// 设置播放模式为实时流模式 - 优化延迟
			if (!PlayM4_SetStreamOpenMode(playPorts_[deviceIdx][channelIdx], STREAME_REALTIME))
			{
				std::cerr << "[TaskVideoCapture] 设备" << (deviceIdx + 1) << "通道" << (channelIdx + 1) << "设置流模式失败" << std::endl;
				return false;
			}

			// 打开流 - 减少缓冲区大小以降低延迟
			if (!PlayM4_OpenStream(playPorts_[deviceIdx][channelIdx], nullptr, 0, 512 * 1024))
			{
				std::cerr << "[TaskVideoCapture] 设备" << (deviceIdx + 1) << "通道" << (channelIdx + 1) << "打开流失败" << std::endl;
				return false;
			}

			// 设置解码回调
			if (!PlayM4_SetDecCallBackExMend(playPorts_[deviceIdx][channelIdx], decodeCallback, 0, 0, 0))
			{
				std::cerr << "[TaskVideoCapture] 设备" << (deviceIdx + 1) << "通道" << (channelIdx + 1) << "设置解码回调失败" << std::endl;
				return false;
			}

			// 开始播放
			if (!PlayM4_Play(playPorts_[deviceIdx][channelIdx], nullptr))
			{
				std::cerr << "[TaskVideoCapture] 设备" << (deviceIdx + 1) << "通道" << (channelIdx + 1) << "开始播放失败" << std::endl;
				return false;
			}

			std::cout << "[TaskVideoCapture] 设备" << (deviceIdx + 1) << "通道" << (channelIdx + 1) << "播放库初始化成功" << std::endl;
		}
	}

	return true;
}

/**
 * @brief 开始预览
 */
bool TaskVideoCapture::startPreview()
{
	for (int deviceIdx = 0; deviceIdx < cameraCount_; deviceIdx++)
	{
		// 只对成功登录的设备启动预览
		if (!deviceLoginSuccess_[deviceIdx])
		{
			std::cout << "[TaskVideoCapture] 跳过设备" << (deviceIdx + 1) << "预览启动（设备未登录成功）" << std::endl;
			continue;
		}

		for (int channelIdx = 0; channelIdx < 2; channelIdx++)
		{
			NET_DVR_PREVIEWINFO previewInfo;
			memset(&previewInfo, 0, sizeof(NET_DVR_PREVIEWINFO));

			previewInfo.hPlayWnd = nullptr; // 不需要窗口句柄，使用回调
			// 使用简单的通道号：1和2（参考VideoCapSDK的实现）
			previewInfo.lChannel = channelIdx + 1; // 通道号（1和2）
			previewInfo.dwStreamType = 0;		   // 主码流
			previewInfo.dwLinkMode = 0;			   // TCP模式
			previewInfo.bBlocked = 0;			   // 非阻塞模式

			// 构造用户数据：使用与参考代码相同的方式
			void *userData = reinterpret_cast<void *>((static_cast<uintptr_t>(deviceIdx) << 8) | channelIdx);

			// 开始实时预览，注册数据回调
			playHandles_[deviceIdx][channelIdx] = NET_DVR_RealPlay_V40(userIDs_[deviceIdx], &previewInfo, dataCallback, userData);

			if (playHandles_[deviceIdx][channelIdx] < 0)
			{
				std::cerr << "[TaskVideoCapture] 设备" << (deviceIdx + 1) << "通道" << (channelIdx + 1) << "开始预览失败，错误码: " << NET_DVR_GetLastError() << std::endl;
				// return false;
			}

			std::cout << "[TaskVideoCapture] 设备" << (deviceIdx + 1) << "通道" << (channelIdx + 1) << "开始预览成功，播放句柄: " << playHandles_[deviceIdx][channelIdx] << std::endl;
		}
	}

	return true;
}

/**
 * @brief 停止预览
 */
void TaskVideoCapture::stopPreview()
{
	for (int deviceIdx = 0; deviceIdx < cameraCount_; deviceIdx++)
	{
		// 只对成功登录的设备停止预览
		if (!deviceLoginSuccess_[deviceIdx])
		{
			continue;
		}

		for (int channelIdx = 0; channelIdx < 2; channelIdx++)
		{
			if (playHandles_[deviceIdx][channelIdx] >= 0)
			{
				NET_DVR_StopRealPlay(playHandles_[deviceIdx][channelIdx]);
				playHandles_[deviceIdx][channelIdx] = -1;
				std::cout << "[TaskVideoCapture] 设备" << (deviceIdx + 1) << "通道" << (channelIdx + 1) << "停止预览" << std::endl;
			}
		}
	}
}

/**
 * @brief 清理资源
 */
void TaskVideoCapture::cleanup()
{
	stopPreview();

	// 停止实时测温功能
	for (int i = 0; i < cameraCount_; ++i)
	{
		stopRealtimeThermometry(i);
	}

	// 清理播放库资源 - 只清理成功登录设备的资源
	for (int deviceIdx = 0; deviceIdx < cameraCount_; deviceIdx++)
	{
		// 只清理成功登录设备的资源
		if (!deviceLoginSuccess_[deviceIdx])
		{
			continue;
		}

		for (int channelIdx = 0; channelIdx < 2; channelIdx++)
		{
			if (playPorts_[deviceIdx][channelIdx] >= 0)
			{
				PlayM4_Stop(playPorts_[deviceIdx][channelIdx]);
				PlayM4_CloseStream(playPorts_[deviceIdx][channelIdx]);
				PlayM4_FreePort(playPorts_[deviceIdx][channelIdx]);

				// 从映射中移除
				{
					std::lock_guard<std::mutex> lock(portMapMutex_);
					portMap_.erase(playPorts_[deviceIdx][channelIdx]);
				}

				playPorts_[deviceIdx][channelIdx] = -1;
				std::cout << "[TaskVideoCapture] 设备" << (deviceIdx + 1) << "通道" << (channelIdx + 1) << "释放播放端口" << std::endl;
			}
		}
	}

	// 登出设备 - 只登出成功登录的设备
	for (int i = 0; i < cameraCount_; i++)
	{
		if (deviceLoginSuccess_[i] && userIDs_[i] >= 0)
		{
			NET_DVR_Logout(userIDs_[i]);
			userIDs_[i] = -1;
			deviceLoginSuccess_[i] = false;
			std::string deviceName = (i == 0) ? "一位端(设备1)" : "二位端(设备2)";
			std::cout << "[TaskVideoCapture] " << deviceName << "登出" << std::endl;
		}
	}

	// 清理SDK
	NET_DVR_Cleanup();
	std::cout << "[TaskVideoCapture] SDK清理完成" << std::endl;
}

/**
 * @brief 线程主函数
 */
void TaskVideoCapture::run()
{
	std::cout << "[TaskVideoCapture] 开始初始化海康SDK..." << std::endl;

	// 初始化SDK
	if (!initializeSDK())
	{
		std::cerr << "[TaskVideoCapture] SDK初始化失败" << std::endl;
		return;
	}

	// 登录设备
	if (!loginDevices())
	{
		std::cerr << "[TaskVideoCapture] 设备登录失败" << std::endl;
		NET_DVR_Cleanup();
		return;
	}

	// ************* 优化后代码段开始 *************
	// 只为成功登录的设备配置热成像参数
	for (int i = 0; i < cameraCount_; ++i)
	{
		if (deviceLoginSuccess_[i])
		{
			std::string deviceName = (i == 0) ? "一位端(设备1)" : "二位端(设备2)";
			if (!configureThermometry(i))
			{
				std::cerr << "[TaskVideoCapture] " << deviceName << " 热成像配置失败，但将继续尝试拉流" << std::endl;
			}
		}
	}

	// 只为成功登录的设备启动实时测温功能
	for (int i = 0; i < cameraCount_; ++i)
	{
		if (deviceLoginSuccess_[i])
		{
			std::string deviceName = (i == 0) ? "一位端(设备1)" : "二位端(设备2)";
			if (!startRealtimeThermometry(i))
			{
				std::cerr << "[TaskVideoCapture] " << deviceName << " 启动实时测温失败，但将继续其他功能" << std::endl;
			}
		}
	}
	// ************* 优化后代码段结束 *************

	// 初始化播放库
	if (!initializePlayback())
	{
		std::cerr << "[TaskVideoCapture] 播放库初始化失败" << std::endl;
		cleanup();
		return;
	}

	// 开始预览
	if (!startPreview())
	{
		std::cerr << "[TaskVideoCapture] 开始预览失败" << std::endl;
		cleanup();
		return;
	}

	// 配置SDK文件切片参数
	if (data_.videoSaveConfig.enableVideoSave)
	{
		configureSDKFileSplit();
	}

	// 启动SDK视频保存（仅为成功登录的设备）
	for (int i = 0; i < cameraCount_; ++i)
	{
		if (deviceLoginSuccess_[i] && data_.videoSaveConfig.enableVideoSave)
		{
			std::string deviceName = (i == 0) ? "一位端(设备1)" : "二位端(设备2)";
			if (startSDKVideoSave(i))
			{
				std::cout << "[TaskVideoCapture] " << deviceName << " SDK视频保存启动成功" << std::endl;
			}
			else
			{
				std::cerr << "[TaskVideoCapture] " << deviceName << " SDK视频保存启动失败" << std::endl;
			}
		}
	}

	// 启动存储空间监控线程
	if (data_.videoSaveConfig.enableVideoSave)
	{
		storageMonitorThread_ = std::thread(&TaskVideoCapture::storageMonitorThread, this);
		std::cout << "[TaskVideoCapture] 存储空间监控线程已启动" << std::endl;
	}

	std::cout << "[TaskVideoCapture] 海康SDK视频捕获启动成功，进入数据处理循环..." << std::endl;

	// 主循环：将帧数据复制到SharedData
	int errorCount = 0;				 // 错误计数器
	const int MAX_ERROR_COUNT = 100; // 最大连续错误数

	while (data_.isRunning)
	{
		try
		{
			bool hasData = false;

			// 复制帧数据到SharedData - 只处理成功登录的设备
			for (int deviceIdx = 0; deviceIdx < cameraCount_; deviceIdx++)
			{
				// 跳过未成功登录的设备
				if (!deviceLoginSuccess_[deviceIdx])
				{
					continue;
				}

				for (int channelIdx = 0; channelIdx < 2; channelIdx++)
				{
					if (frameMutexes_[deviceIdx][channelIdx])
					{
						std::lock_guard<std::mutex> frameLock(*frameMutexes_[deviceIdx][channelIdx]);
						if (!frameBuffers_[deviceIdx][channelIdx].empty())
						{
							cv::Mat frame = frameBuffers_[deviceIdx][channelIdx].clone();
							hasData = true;

							// 根据设备和通道分发到对应的SharedData字段
							if (deviceIdx == 0) // 第一个设备
							{
								if (channelIdx == 0) // 通道1（可见光）
								{
									std::lock_guard<std::mutex> lock(data_.visible_mutex_1);
									frame.copyTo(data_.visible_video_frame_1);
									// 保存一帧图片
									cv::imwrite("visible_frame_1.jpg", data_.visible_video_frame_1);
								}
								else // 通道2（热成像）
								{
									std::lock_guard<std::mutex> lock(data_.thermal_mutex_1);
									frame.copyTo(data_.thermal_video_frame_1);
									cv::imwrite("thermal_frame_1.jpg", data_.thermal_video_frame_1);
								}
							}
							else if (deviceIdx == 1 && cameraCount_ == 2) // 第二个设备（仅在双设备模式下）
							{
								if (channelIdx == 0) // 通道1（可见光）
								{
									std::lock_guard<std::mutex> lock(data_.visible_mutex_2);
									frame.copyTo(data_.visible_video_frame_2);
								}
								else // 通道2（热成像）
								{
									std::lock_guard<std::mutex> lock(data_.thermal_mutex_2);
									frame.copyTo(data_.thermal_video_frame_2);
								}
							}
						}
					}
				}
			}

			// 如果有数据，重置错误计数
			if (hasData)
			{
				errorCount = 0;
			}

			// 适当延迟，避免过度占用CPU
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}
		catch (const std::exception &e)
		{
			std::cerr << "[TaskVideoCapture] 数据处理异常: " << e.what() << std::endl;
			errorCount++;

			if (errorCount > MAX_ERROR_COUNT)
			{
				std::cerr << "[TaskVideoCapture] 连续错误过多，退出循环" << std::endl;
				break;
			}

			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}
		catch (...)
		{
			std::cerr << "[TaskVideoCapture] 未知异常" << std::endl;
			errorCount++;

			if (errorCount > MAX_ERROR_COUNT)
			{
				std::cerr << "[TaskVideoCapture] 连续错误过多，退出循环" << std::endl;
				break;
			}

			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}
	}

	std::cout << "[TaskVideoCapture] 数据处理循环已退出，开始清理资源..." << std::endl;
	cleanup();
}

/**
 * @brief 异常回调函数
 */
void CALLBACK TaskVideoCapture::exceptionCallback(DWORD dwType, LONG lUserID, LONG lHandle, void *pUser)
{
	switch (dwType)
	{
	case EXCEPTION_RECONNECT:
		std::cout << "[TaskVideoCapture] 预览重连，时间: " << time(NULL) << std::endl;
		break;
	case EXCEPTION_ALARMRECONNECT:
		std::cout << "[TaskVideoCapture] 报警重连，时间: " << time(NULL) << std::endl;
		break;
	case EXCEPTION_SERIALRECONNECT:
		std::cout << "[TaskVideoCapture] 串口重连，时间: " << time(NULL) << std::endl;
		break;
	case 32771: // 网络断开或连接异常
		std::cout << "[TaskVideoCapture] 网络连接异常，用户ID: " << lUserID << "，句柄: " << lHandle << std::endl;
		// 这种异常通常是网络不稳定导致的，可以忽略或尝试重连
		break;
	case 32769: // 网络丢包
		std::cout << "[TaskVideoCapture] 网络丢包异常，用户ID: " << lUserID << std::endl;
		break;
	case 32770: // 网络超时
		std::cout << "[TaskVideoCapture] 网络超时异常，用户ID: " << lUserID << std::endl;
		break;
	default:
		std::cout << "[TaskVideoCapture] 未知异常类型: " << dwType << "，用户ID: " << lUserID << "，句柄: " << lHandle << std::endl;
		break;
	}
}

/**
 * @brief 实时数据回调函数
 */
void CALLBACK TaskVideoCapture::dataCallback(LONG lPlayHandle, DWORD dwDataType,
											 BYTE *pBuffer, DWORD dwBufSize, void *pUser)
{
	// 解析用户数据获取设备和通道信息
	uintptr_t userValue = reinterpret_cast<uintptr_t>(pUser);
	int deviceIdx = static_cast<int>(userValue >> 8);
	int channelIdx = static_cast<int>(userValue & 0xFF);

	// 只处理视频流数据
	if (dwDataType != NET_DVR_STREAMDATA || !pBuffer || dwBufSize == 0)
	{
		return;
	}

	// 获取实例指针
	TaskVideoCapture *instance = nullptr;
	{
		std::lock_guard<std::mutex> lock(instanceMutex_);
		instance = instance_;
	}

	if (!instance || deviceIdx >= instance->cameraCount_ || channelIdx >= 2)
	{
		return;
	}

	// 将数据送入播放库进行解码
	if (instance->playPorts_[deviceIdx][channelIdx] >= 0)
	{
		PlayM4_InputData(instance->playPorts_[deviceIdx][channelIdx], pBuffer, dwBufSize);
	}
}

/**
 * @brief 解码回调函数：将YUV数据转换为BGR格式的Mat
 */
void CALLBACK TaskVideoCapture::decodeCallback(long nPort, char *pBuf, long nSize,
											   FRAME_INFO *pFrameInfo, long nUser, long nReserved2)
{
	// 基本参数检查
	if (!pBuf || nSize <= 0 || !pFrameInfo)
	{
		return;
	}

	// 通过端口查找对应的设备和通道
	int deviceIdx = -1, channelIdx = -1;
	{
		std::lock_guard<std::mutex> lock(portMapMutex_);
		auto it = portMap_.find(nPort);
		if (it != portMap_.end())
		{
			deviceIdx = it->second.first;
			channelIdx = it->second.second;
		}
	}

	if (deviceIdx < 0 || channelIdx < 0)
	{
		return;
	}

	// 获取实例指针
	TaskVideoCapture *instance = nullptr;
	{
		std::lock_guard<std::mutex> lock(instanceMutex_);
		instance = instance_;
	}

	if (!instance || deviceIdx >= instance->cameraCount_ || channelIdx >= 2)
	{
		return;
	}

	// 只处理YV12格式的YUV数据
	if (pFrameInfo->nType != T_YV12)
	{
		return;
	}

	try
	{
		// 验证帧信息的合理性
		if (pFrameInfo->nWidth <= 0 || pFrameInfo->nHeight <= 0 ||
			pFrameInfo->nWidth > 4096 || pFrameInfo->nHeight > 4096)
		{
			return;
		}

		// 计算所需的缓冲区大小
		int expectedSize = pFrameInfo->nWidth * pFrameInfo->nHeight * 3 / 2;
		if (nSize < expectedSize)
		{
			return;
		}

		// 创建YUV Mat
		cv::Mat yuvMat(pFrameInfo->nHeight + pFrameInfo->nHeight / 2,
					   pFrameInfo->nWidth, CV_8UC1, (uchar *)pBuf);

		// 转换为BGR格式
		cv::Mat bgrMat;
		cv::cvtColor(yuvMat, bgrMat, cv::COLOR_YUV2BGR_YV12);

		// 验证转换结果
		if (bgrMat.empty() || bgrMat.cols != pFrameInfo->nWidth || bgrMat.rows != pFrameInfo->nHeight)
		{
			return;
		}

		// 存储到帧缓冲区
		if (instance->frameMutexes_[deviceIdx][channelIdx])
		{
			std::lock_guard<std::mutex> lock(*instance->frameMutexes_[deviceIdx][channelIdx]);
			instance->frameBuffers_[deviceIdx][channelIdx] = bgrMat.clone();
		}
	}
	catch (const cv::Exception &e)
	{
		std::cerr << "[TaskVideoCapture] OpenCV解码异常: " << e.what() << std::endl;
	}
	catch (const std::exception &e)
	{
		std::cerr << "[TaskVideoCapture] 解码回调标准异常: " << e.what() << std::endl;
	}
	catch (...)
	{
		std::cerr << "[TaskVideoCapture] 解码回调未知异常" << std::endl;
	}
}

/**
 * @brief 启动实时测温数据获取
 */
bool TaskVideoCapture::startRealtimeThermometry(int deviceIdx)
{
	if (deviceIdx < 0 || deviceIdx >= cameraCount_ || userIDs_[deviceIdx] < 0)
	{
		std::cerr << "[TaskVideoCapture] 设备" << (deviceIdx + 1) << "无效或未登录，无法启动测温" << std::endl;
		return false;
	}

	std::cout << "[TaskVideoCapture] 开始为设备 " << (deviceIdx + 1) << " 启动实时测温..." << std::endl;

	// 构造测温配置参数
	NET_DVR_REALTIME_THERMOMETRY_COND thermCond;
	memset(&thermCond, 0, sizeof(thermCond));
	thermCond.dwSize = sizeof(NET_DVR_REALTIME_THERMOMETRY_COND);
	thermCond.byRuleID = 0; // 0代表获取全部规则，具体规则ID从1开始
	thermCond.dwChan = 2;	// 热成像通道通常为2

	// 启动实时测温长连接
	LONG thermHandle = NET_DVR_StartRemoteConfig(userIDs_[deviceIdx],
												 NET_DVR_GET_REALTIME_THERMOMETRY,
												 &thermCond,
												 sizeof(thermCond),
												 thermometryCallback,
												 reinterpret_cast<void *>(static_cast<uintptr_t>(deviceIdx)));

	if (thermHandle < 0)
	{
		DWORD errorCode = NET_DVR_GetLastError();
		std::cout << "[TaskVideoCapture] 设备 " << (deviceIdx + 1) << " 启动实时测温失败，错误码: " << errorCode << std::endl;
		return false;
	}

	// 保存句柄和状态
	thermometryHandles_[deviceIdx] = thermHandle;
	thermometryActive_[deviceIdx] = true;

	std::cout << "[TaskVideoCapture] 设备 " << (deviceIdx + 1) << " 实时测温启动成功，句柄: " << thermHandle << std::endl;
	return true;
}

/**
 * @brief 停止实时测温数据获取
 */
void TaskVideoCapture::stopRealtimeThermometry(int deviceIdx)
{
	if (deviceIdx < 0 || deviceIdx >= cameraCount_)
	{
		return;
	}

	if (thermometryActive_[deviceIdx] && thermometryHandles_[deviceIdx] >= 0)
	{
		if (NET_DVR_StopRemoteConfig(thermometryHandles_[deviceIdx]))
		{
			std::cout << "[TaskVideoCapture] 设备 " << (deviceIdx + 1) << " 停止实时测温成功" << std::endl;
		}
		else
		{
			DWORD errorCode = NET_DVR_GetLastError();
			std::cout << "[TaskVideoCapture] 设备 " << (deviceIdx + 1) << " 停止实时测温失败，错误码: " << errorCode << std::endl;
		}

		thermometryHandles_[deviceIdx] = -1;
		thermometryActive_[deviceIdx] = false;
	}
}

/**
 * @brief 实时测温数据回调函数
 */
void CALLBACK TaskVideoCapture::thermometryCallback(DWORD dwType, void *lpBuffer, DWORD dwBufLen, void *pUserData)
{
	// 检查回调类型，只处理数据类型回调
	if (dwType != NET_SDK_CALLBACK_TYPE_DATA)
	{
		if (dwType == NET_SDK_CALLBACK_TYPE_STATUS)
		{
			DWORD dwStatus = *(DWORD *)lpBuffer;
			if (dwStatus == NET_SDK_CALLBACK_STATUS_SUCCESS)
			{
				std::cout << "[TaskVideoCapture] 实时测温状态: 成功" << std::endl;
			}
			else if (dwStatus == NET_SDK_CALLBACK_STATUS_FAILED)
			{
				DWORD dwErrCode = *(DWORD *)((char *)lpBuffer + 4);
				std::cout << "[TaskVideoCapture] 实时测温失败，错误码: " << dwErrCode << std::endl;
			}
		}
		return;
	}

	// 解析设备索引
	uintptr_t deviceIdx = reinterpret_cast<uintptr_t>(pUserData);

	// 获取实例指针
	TaskVideoCapture *instance = nullptr;
	{
		std::lock_guard<std::mutex> lock(instanceMutex_);
		instance = instance_;
	}

	if (!instance || !lpBuffer || dwBufLen == 0)
	{
		return;
	}

	// 验证设备索引有效性
	if (deviceIdx >= static_cast<uintptr_t>(instance->cameraCount_))
	{
		return;
	}

	try
	{
		// 解析测温数据
		NET_DVR_THERMOMETRY_UPLOAD *thermData = reinterpret_cast<NET_DVR_THERMOMETRY_UPLOAD *>(lpBuffer);

		// 解析时间戳
		// 相对时标（带时区，如东八区时间）
		DWORD relativeTime = thermData->dwRelativeTime;
		int relYear = GET_YEAR(relativeTime);
		int relMonth = GET_MONTH(relativeTime);
		int relDay = GET_DAY(relativeTime);
		int relHour = GET_HOUR(relativeTime);
		int relMinute = GET_MINUTE(relativeTime);
		int relSecond = GET_SECOND(relativeTime);

		// 构造温度数据结构
		RealTimeTemperatureData tempData;
		tempData.ruleID = thermData->byRuleID;
		tempData.ruleName = std::string(thermData->szRuleName);
		tempData.timestamp = thermData->dwAbsTime;
		tempData.channelNo = thermData->dwChan;

		// 生成格式化的时间字符串
		std::stringstream relTimeStream, absTimeStream;
		relTimeStream << relYear << "-" << std::setfill('0') << std::setw(2) << relMonth << "-" << std::setw(2) << relDay
					  << " " << std::setw(2) << relHour << ":" << std::setw(2) << relMinute << ":" << std::setw(2) << relSecond;
		tempData.relativeTimeStr = relTimeStream.str();
		// 根据测温规则类型解析温度数据
		if (thermData->byRuleCalibType == 0) // 点测温
		{
			float pointTemp = thermData->struPointThermCfg.fTemperature;
			tempData.highestTemperature = pointTemp;
			tempData.lowestTemperature = pointTemp;
			tempData.centerTemperature = pointTemp;
			tempData.isValid = true;

			std::cout << "[TaskVideoCapture] 设备 " << (deviceIdx + 1)
					  << " 点测温数据 - 温度: " << pointTemp << "°C"
					  << ", 规则ID: " << (int)thermData->byRuleID
					  << ", 相对时标: " << relYear << "-" << std::setfill('0') << std::setw(2) << relMonth << "-" << std::setw(2) << relDay
					  << " " << std::setw(2) << relHour << ":" << std::setw(2) << relMinute << ":" << std::setw(2) << relSecond
					  << std::endl;
		}
		else if (thermData->byRuleCalibType == 1 || thermData->byRuleCalibType == 2) // 框测温或线测温
		{
			tempData.highestTemperature = thermData->struLinePolygonThermCfg.fMaxTemperature;
			tempData.lowestTemperature = thermData->struLinePolygonThermCfg.fMinTemperature;
			tempData.centerTemperature = thermData->struLinePolygonThermCfg.fAverageTemperature;
			tempData.isValid = true;
			// std::cout << "[TaskVideoCapture] 设备 " << (deviceIdx + 1)
			// 		  << " " << (thermData->byRuleCalibType == 1 ? "框" : "线") << "测温数据"
			// 		  << " - 最高温: " << tempData.highestTemperature << "°C"
			// 		  << ", 最低温: " << tempData.lowestTemperature << "°C"
			// 		  << ", 平均温: " << tempData.centerTemperature << "°C"
			// 		  << ", 温差: " << thermData->struLinePolygonThermCfg.fTemperatureDiff << "°C"
			// 		  << ", 规则ID: " << (int)thermData->byRuleID
			// 		  << ", 相对时标: " << relYear << "-" << std::setfill('0') << std::setw(2) << relMonth << "-" << std::setw(2) << relDay
			// 		  << " " << std::setw(2) << relHour << ":" << std::setw(2) << relMinute << ":" << std::setw(2) << relSecond
			// 		  << std::endl;
		}
		else
		{
			std::cout << "[TaskVideoCapture] 设备 " << (deviceIdx + 1)
					  << " 未知测温类型: " << (int)thermData->byRuleCalibType
					  << ", 规则ID: " << (int)thermData->byRuleID
					  << std::endl;
			tempData.isValid = false;
		}

		// 只有当数据有效时才存储到SharedData
		if (tempData.isValid)
		{
			if (deviceIdx == 0) // 第一个设备
			{
				std::lock_guard<std::mutex> lock(instance->data_.realtimeTemp_mutex_1);
				instance->data_.realtimeTemp_1 = tempData;
			}
			else if (deviceIdx == 1) // 第二个设备
			{
				std::lock_guard<std::mutex> lock(instance->data_.realtimeTemp_mutex_2);
				instance->data_.realtimeTemp_2 = tempData;
			}
		}
	}
	catch (const std::exception &e)
	{
		std::cerr << "[TaskVideoCapture] 测温回调异常: " << e.what() << std::endl;
	}
	catch (...)
	{
		std::cerr << "[TaskVideoCapture] 测温回调未知异常" << std::endl;
	}
}

// ==================== SDK视频保存功能实现 ====================

/**
 * @brief 配置SDK文件切片参数
 */
bool TaskVideoCapture::configureSDKFileSplit()
{
	std::cout << "[TaskVideoCapture] 配置SDK文件切片参数..." << std::endl;

	NET_DVR_LOCAL_GENERAL_CFG struGeneral = {0};

	// 获取当前配置
	if (!NET_DVR_GetSDKLocalCfg(NET_DVR_LOCAL_CFG_TYPE_GENERAL, &struGeneral))
	{
		DWORD errorCode = NET_DVR_GetLastError();
		std::cout << "[TaskVideoCapture] 获取SDK本地配置失败，错误码: " << errorCode << std::endl;
		// 使用默认配置继续
		memset(&struGeneral, 0, sizeof(struGeneral));
	}

	// 设置切片参数
	struGeneral.byNotSplitRecordFile = 0;															  // 0=启用切片，1=不切片
	struGeneral.i64FileSize = static_cast<UINT64>(data_.videoSaveConfig.maxFileSizeMB) * 1024 * 1024; // 转换为字节

	// 应用配置
	if (!NET_DVR_SetSDKLocalCfg(NET_DVR_LOCAL_CFG_TYPE_GENERAL, &struGeneral))
	{
		DWORD errorCode = NET_DVR_GetLastError();
		std::cerr << "[TaskVideoCapture] 设置SDK本地配置失败，错误码: " << errorCode << std::endl;
		return false;
	}

	std::cout << "[TaskVideoCapture] SDK文件切片配置成功，文件大小限制: "
			  << data_.videoSaveConfig.maxFileSizeMB << "MB" << std::endl;
	return true;
}

/**
 * @brief 生成视频文件名
 */
std::string TaskVideoCapture::generateVideoFilePath(int deviceIdx)
{
	auto now = std::chrono::system_clock::now();
	auto time_t = std::chrono::system_clock::to_time_t(now);

	std::stringstream ss;
	struct tm timeInfo;
	localtime_s(&timeInfo, &time_t);

	// 格式：Camera1_20250128_143025.mp4
	ss << data_.videoSaveConfig.videoSavePath;
	if (ss.str().back() != '/' && ss.str().back() != '\\')
	{
		ss << "/";
	}

	ss << "Camera" << (deviceIdx + 1) << "_Visible_"
	   << std::put_time(&timeInfo, "%Y%m%d_%H%M%S")
	   << ".mp4";

	return ss.str();
}

/**
 * @brief 启动SDK视频保存
 */
bool TaskVideoCapture::startSDKVideoSave(int deviceIdx)
{
	if (deviceIdx < 0 || deviceIdx >= cameraCount_)
	{
		std::cerr << "[TaskVideoCapture] 无效的设备索引: " << deviceIdx << std::endl;
		return false;
	}

	if (!deviceLoginSuccess_[deviceIdx])
	{
		std::cerr << "[TaskVideoCapture] 设备" << (deviceIdx + 1) << "未登录成功，无法启动视频保存" << std::endl;
		return false;
	}

	if (videoSaveActive_[deviceIdx])
	{
		std::cout << "[TaskVideoCapture] 设备" << (deviceIdx + 1) << "视频保存已在运行" << std::endl;
		return true;
	}

	// 创建保存目录
	try
	{
		std::filesystem::create_directories(data_.videoSaveConfig.videoSavePath);
	}
	catch (const std::exception &e)
	{
		std::cerr << "[TaskVideoCapture] 创建保存目录失败: " << e.what() << std::endl;
		return false;
	}

	// 标记为活跃并启动线程
	videoSaveActive_[deviceIdx] = true;
	videoSaveThreads_[deviceIdx] = std::thread(&TaskVideoCapture::sdkVideoSaveThread, this, deviceIdx);

	std::cout << "[TaskVideoCapture] 设备" << (deviceIdx + 1) << "视频保存线程已启动" << std::endl;
	return true;
}

/**
 * @brief 停止SDK视频保存
 */
void TaskVideoCapture::stopSDKVideoSave(int deviceIdx)
{
	if (deviceIdx < 0 || deviceIdx >= cameraCount_)
	{
		return;
	}

	if (!videoSaveActive_[deviceIdx])
	{
		return;
	}

	videoSaveActive_[deviceIdx] = false;

	// 等待线程结束
	if (videoSaveThreads_[deviceIdx].joinable())
	{
		videoSaveThreads_[deviceIdx].join();
		std::cout << "[TaskVideoCapture] 设备" << (deviceIdx + 1) << "视频保存线程已停止" << std::endl;
	}
}

/**
 * @brief SDK视频保存线程函数
 * 创建独立的预览句柄用于视频保存，避免阻塞推流
 */
void TaskVideoCapture::sdkVideoSaveThread(int deviceIdx)
{
	std::string deviceName = (deviceIdx == 0) ? "一位端" : "二位端";
	std::cout << "[TaskVideoCapture] " << deviceName << "视频保存线程开始运行..." << std::endl;

	// 创建独立的预览句柄用于视频保存（不能复用推流的预览句柄）
	NET_DVR_PREVIEWINFO previewInfo;
	memset(&previewInfo, 0, sizeof(NET_DVR_PREVIEWINFO));

	previewInfo.hPlayWnd = nullptr; // 不需要窗口句柄
	previewInfo.lChannel = 1;		// 通道1（可见光）
	previewInfo.dwStreamType = 0;	// 主码流
	previewInfo.dwLinkMode = 0;		// TCP模式
	previewInfo.bBlocked = 1;		// 阻塞模式（视频保存建议用阻塞）

	// 开始实时预览（用于视频保存）
	LONG saveHandle = NET_DVR_RealPlay_V40(userIDs_[deviceIdx], &previewInfo, nullptr, nullptr);

	if (saveHandle < 0)
	{
		DWORD errorCode = NET_DVR_GetLastError();
		std::cerr << "[TaskVideoCapture] " << deviceName << "创建视频保存预览句柄失败，错误码: "
				  << errorCode << std::endl;
		videoSaveActive_[deviceIdx] = false;
		return;
	}

	videoSaveHandles_[deviceIdx] = saveHandle;
	std::cout << "[TaskVideoCapture] " << deviceName << "视频保存预览句柄创建成功: " << saveHandle << std::endl;

	// 生成视频文件路径（仅一次，SDK会自动切片并命名为 *_1.mp4, *_2.mp4...）
	std::string videoPath = generateVideoFilePath(deviceIdx);
	std::cout << "[TaskVideoCapture] " << deviceName << "开始录制视频: " << videoPath << std::endl;
	std::cout << "[TaskVideoCapture] SDK将自动按 " << data_.videoSaveConfig.maxFileSizeMB
			  << "MB 切片文件" << std::endl;

	// 调用SDK保存接口（STREAM_PS=0x1, 适用于MP4容器）
	if (!NET_DVR_SaveRealData_V30(saveHandle, 0x1, const_cast<char *>(videoPath.c_str())))
	{
		DWORD errorCode = NET_DVR_GetLastError();
		std::cerr << "[TaskVideoCapture] " << deviceName << "启动视频保存失败，错误码: "
				  << errorCode << std::endl;

		// 停止预览并清理
		NET_DVR_StopRealPlay(saveHandle);
		videoSaveHandles_[deviceIdx] = -1;
		videoSaveActive_[deviceIdx] = false;
		return;
	}

	std::cout << "[TaskVideoCapture] " << deviceName << "视频录制已启动，SDK自动管理文件切片..." << std::endl;

	// 持续录制直到停止信号
	while (videoSaveActive_[deviceIdx] && !shouldStopVideoSave_)
	{
		std::this_thread::sleep_for(std::chrono::seconds(1));
	}

	// 停止保存
	std::cout << "[TaskVideoCapture] " << deviceName << "正在停止视频录制..." << std::endl;
	if (!NET_DVR_StopSaveRealData(saveHandle))
	{
		DWORD errorCode = NET_DVR_GetLastError();
		std::cerr << "[TaskVideoCapture] " << deviceName << "停止视频保存失败，错误码: "
				  << errorCode << std::endl;
	}
	else
	{
		std::cout << "[TaskVideoCapture] " << deviceName << "视频录制已停止" << std::endl;
	}

	// 停止预览句柄
	if (!NET_DVR_StopRealPlay(saveHandle))
	{
		DWORD errorCode = NET_DVR_GetLastError();
		std::cerr << "[TaskVideoCapture] " << deviceName << "停止视频保存预览失败，错误码: "
				  << errorCode << std::endl;
	}

	videoSaveHandles_[deviceIdx] = -1;
	std::cout << "[TaskVideoCapture] " << deviceName << "视频保存线程已退出" << std::endl;
}

// ==================== 存储空间管理功能实现 ====================

/**
 * @brief 存储空间监控线程函数
 */
void TaskVideoCapture::storageMonitorThread()
{
	std::cout << "[TaskVideoCapture] 存储空间监控线程开始运行..." << std::endl;

	const int CHECK_INTERVAL_SECONDS = 60; // 每60秒检查一次
	const size_t BYTES_PER_GB = 1024ULL * 1024ULL * 1024ULL;

	while (!shouldStopVideoSave_)
	{
		try
		{
			// 计算当前存储大小
			size_t currentSize = calculateDirectorySize(data_.videoSaveConfig.videoSavePath);
			size_t maxSizeBytes = static_cast<size_t>(data_.videoSaveConfig.maxStorageGB) * BYTES_PER_GB;

			double currentSizeGB = static_cast<double>(currentSize) / BYTES_PER_GB;

			// 定期输出存储使用情况
			static int logCounter = 0;
			if (++logCounter >= 10) // 每10分钟输出一次
			{
				std::cout << "[TaskVideoCapture] 存储使用情况: "
						  << std::fixed << std::setprecision(2) << currentSizeGB << "GB / "
						  << data_.videoSaveConfig.maxStorageGB << "GB" << std::endl;
				logCounter = 0;
			}

			// 检查是否超过限制
			if (currentSize > maxSizeBytes)
			{
				std::cout << "[TaskVideoCapture] 存储空间超限: "
						  << std::fixed << std::setprecision(2) << currentSizeGB << "GB / "
						  << data_.videoSaveConfig.maxStorageGB << "GB，开始清理..." << std::endl;

				size_t cleanupSizeBytes = static_cast<size_t>(data_.videoSaveConfig.cleanupSizeGB) * BYTES_PER_GB;
				size_t cleanedSize = cleanupOldVideos(cleanupSizeBytes);

				double cleanedSizeGB = static_cast<double>(cleanedSize) / BYTES_PER_GB;
				std::cout << "[TaskVideoCapture] 清理完成，已删除: "
						  << std::fixed << std::setprecision(2) << cleanedSizeGB << "GB" << std::endl;
			}
		}
		catch (const std::exception &e)
		{
			std::cerr << "[TaskVideoCapture] 存储监控异常: " << e.what() << std::endl;
		}

		// 等待指定间隔
		for (int i = 0; i < CHECK_INTERVAL_SECONDS && !shouldStopVideoSave_; ++i)
		{
			std::this_thread::sleep_for(std::chrono::seconds(1));
		}
	}

	std::cout << "[TaskVideoCapture] 存储空间监控线程已退出" << std::endl;
}

/**
 * @brief 计算目录总大小
 */
size_t TaskVideoCapture::calculateDirectorySize(const std::string &path)
{
	size_t totalSize = 0;

	try
	{
		if (!std::filesystem::exists(path) || !std::filesystem::is_directory(path))
		{
			return 0;
		}

		for (const auto &entry : std::filesystem::recursive_directory_iterator(path))
		{
			if (entry.is_regular_file())
			{
				totalSize += entry.file_size();
			}
		}
	}
	catch (const std::filesystem::filesystem_error &e)
	{
		std::cerr << "[TaskVideoCapture] 计算目录大小失败: " << e.what() << std::endl;
	}

	return totalSize;
}

/**
 * @brief 清理旧视频文件
 */
size_t TaskVideoCapture::cleanupOldVideos(size_t targetCleanupSize)
{
	auto videoFiles = getVideoFilesSortedByTime();
	size_t cleanedSize = 0;

	std::cout << "[TaskVideoCapture] 开始清理旧视频文件，目标清理: "
			  << (targetCleanupSize / (1024 * 1024 * 1024)) << "GB" << std::endl;

	for (const auto &filePath : videoFiles)
	{
		if (cleanedSize >= targetCleanupSize)
		{
			break;
		}

		try
		{
			size_t fileSize = std::filesystem::file_size(filePath);

			// 检查文件是否正在使用（被保存线程使用）
			bool isInUse = false;
			for (int i = 0; i < cameraCount_; ++i)
			{
				// 简单检查：如果文件名包含当前时间戳，可能正在使用
				// 这里使用更保守的策略：跳过最近1小时内的文件
				auto fileTime = std::filesystem::last_write_time(filePath);
				auto now = std::filesystem::file_time_type::clock::now();
				auto age = std::chrono::duration_cast<std::chrono::hours>(now - fileTime);

				if (age.count() < 1)
				{
					isInUse = true;
					break;
				}
			}

			if (!isInUse)
			{
				std::filesystem::remove(filePath);
				cleanedSize += fileSize;
				std::cout << "[TaskVideoCapture] 删除文件: " << filePath.filename().string()
						  << " (大小: " << (fileSize / (1024 * 1024)) << "MB)" << std::endl;
			}
		}
		catch (const std::filesystem::filesystem_error &e)
		{
			std::cerr << "[TaskVideoCapture] 删除文件失败: " << filePath << ", 错误: " << e.what() << std::endl;
		}
	}

	std::cout << "[TaskVideoCapture] 清理完成，实际清理: "
			  << (cleanedSize / (1024 * 1024 * 1024)) << "GB" << std::endl;

	return cleanedSize;
}

/**
 * @brief 获取按时间排序的视频文件列表
 */
std::vector<std::filesystem::path> TaskVideoCapture::getVideoFilesSortedByTime()
{
	std::vector<std::filesystem::path> videoFiles;

	try
	{
		if (!std::filesystem::exists(data_.videoSaveConfig.videoSavePath))
		{
			return videoFiles;
		}

		for (const auto &entry : std::filesystem::directory_iterator(data_.videoSaveConfig.videoSavePath))
		{
			if (entry.is_regular_file())
			{
				std::string extension = entry.path().extension().string();
				std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

				// 只处理视频文件
				if (extension == ".mp4" || extension == ".avi" || extension == ".mkv")
				{
					videoFiles.push_back(entry.path());
				}
			}
		}

		// 按修改时间排序（从旧到新）
		std::sort(videoFiles.begin(), videoFiles.end(),
				  [](const std::filesystem::path &a, const std::filesystem::path &b)
				  {
					  return std::filesystem::last_write_time(a) < std::filesystem::last_write_time(b);
				  });
	}
	catch (const std::filesystem::filesystem_error &e)
	{
		std::cerr << "[TaskVideoCapture] 获取视频文件列表失败: " << e.what() << std::endl;
	}

	return videoFiles;
}