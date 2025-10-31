#include "ThreadManager.h"

/**
 * @brief ThreadManager 类的构造函数
 * 功能：初始化海康威视红外温度识别系统的多线程管理器
 * 参数说明：
 *   cameraCount: 摄像头数量（1或2），控制处理的视频流数量
 *   deviceConfigs: 海康设备配置列表，包含IP、端口、用户名、密码等信息
 *   sharedData: 多线程共享数据对象，作为各任务线程间的数据交换中心
 *   rtspUrls: 本地RTSP服务器推流地址列表，用于向外部客户端提供处理后的视频流
 *   trackingConfig: 目标追踪配置参数
 */
ThreadManager::ThreadManager(int cameraCount,
							 const std::vector<nlohmann::json> &deviceConfigs,
							 SharedData &sharedData,
							 const std::vector<std::string> &rtspUrls,
							 const ObjectTrackingConfig &trackingConfig,
							 int streamWidth,
							 int streamHeight,
							 int streamFps)
	: sharedData_(sharedData) // 保存共享数据引用
{
	std::cout << "[ThreadManager] 初始化多线程管理器，摄像头数量: " << cameraCount << std::endl;

	// 初始化6个专门化任务线程，构建完整的视频处理流水线：

	// 1. 视频捕获任务：使用海康SDK原生拉流，替代FFmpeg方式
	taskVideo_ = std::make_unique<TaskVideoCapture>(cameraCount, deviceConfigs, sharedData);

	// 2. 热成像数据捕获任务：通过海康SDK获取温度矩阵数据（RTSP无法提供）
	// 注意：TaskThermalCapture将在TaskVideoCapture登录成功后再创建
	taskThermal_ = nullptr; // 暂时设为空指针

	// 3. 显示处理任务：将温度数据与视频帧融合，进行目标识别和可视化处理
	taskDisplay_ = std::make_unique<TaskDisplay>(sharedData);

	// 4. RTSP推流任务：将处理后的视频通过FFmpeg推送到本地RTSP服务器
	taskRTSPStream_ = std::make_unique<TaskRTSPStream>(sharedData, rtspUrls, streamWidth, streamHeight, streamFps);

	// 5. 热成像检测任务：检测高温物体并设置标志位（移除上报逻辑）
	taskLocating_ = std::make_unique<TaskLocating>(sharedData);

	// 6. 目标追踪任务：对可见光视频流进行YOLO检测、ByteTrack追踪和计数（移除上报逻辑）
	taskObjectTracking_ = std::make_unique<TaskObjectTracking>(sharedData, trackingConfig);

	// 7. 统一定位上报任务：定期检查检测标志位并执行统一上报
	// 传递完整的配置对象
	taskLocationReporter_ = std::make_unique<TaskLocationReporter>(sharedData, trackingConfig);

	std::cout << "[ThreadManager] 所有任务线程初始化完成" << std::endl;
}

/**
 * @brief ThreadManager 类的析构函数
 * 功能：确保所有任务线程安全退出，防止资源泄漏
 */
ThreadManager::~ThreadManager()
{
	std::cout << "[ThreadManager] 开始析构，停止所有线程..." << std::endl;
	stopAll(); // 统一停止所有线程，保证系统有序关闭
	std::cout << "[ThreadManager] 析构完成" << std::endl;
}

/**
 * @brief 启动所有任务线程
 * 功能：按照数据流向顺序启动线程，构建完整的视频处理流水线
 * 流程：视频采集 → 温度获取 → 数据处理 → 目标追踪 → RTSP推流 → 定位上报
 */
void ThreadManager::startAll()
{
	std::cout << "[ThreadManager] 开始启动所有任务线程..." << std::endl;

	// 1. 启动视频捕获线程（数据源）- 包含海康SDK登录逻辑
	std::cout << "[ThreadManager] 启动视频捕获线程..." << std::endl;
	taskVideo_->start();

	// 等待视频捕获线程完成设备登录，然后创建热成像任务
	std::this_thread::sleep_for(std::chrono::seconds(3)); // 给足够时间完成登录

	// 2. 创建并启动热成像数据捕获线程（温度数据源）
	std::cout << "[ThreadManager] 获取设备登录信息并创建热成像数据捕获任务..." << std::endl;
	std::vector<LONG> userIDs = taskVideo_->getDeviceUserIDs();
	taskThermal_ = std::make_unique<TaskThermalCapture>(userIDs, sharedData_);
	std::cout << "[ThreadManager] 启动热成像数据捕获线程..." << std::endl;
	taskThermal_->start();

	// 3. 启动显示处理线程（数据处理）
	std::cout << "[ThreadManager] 启动显示处理线程..." << std::endl;
	taskDisplay_->start();

	// 4. 启动目标追踪线程（可见光视频处理）
	std::cout << "[ThreadManager] 启动目标追踪线程..." << std::endl;
	taskObjectTracking_->start();

	// 5. 启动RTSP推流线程（数据输出）
	std::cout << "[ThreadManager] 启动RTSP推流线程..." << std::endl;
	taskRTSPStream_->start();

	// 6. 启动热成像检测线程（只负责检测，不负责上报）
	std::cout << "[ThreadManager] 启动热成像检测线程..." << std::endl;
	taskLocating_->start();

	// 7. 启动统一定位上报线程（统一处理所有上报逻辑）
	std::cout << "[ThreadManager] 启动统一定位上报线程..." << std::endl;
	taskLocationReporter_->start();

	std::cout << "[ThreadManager] 所有任务线程启动完成" << std::endl;
}

/**
 * @brief 停止所有任务线程
 * 功能：按照与启动相反的顺序停止线程，确保数据处理流水线有序关闭
 * 顺序：先停止输出端，再停止处理端，最后停止输入端，避免数据丢失
 */
void ThreadManager::stopAll()
{
	std::cout << "[ThreadManager] 开始停止所有任务线程..." << std::endl;

	// 1. 停止统一定位上报线程（优先停止外部通信）
	if (taskLocationReporter_)
	{
		std::cout << "[ThreadManager] 停止统一定位上报线程..." << std::endl;
		taskLocationReporter_->stop();
	}

	// 2. 停止热成像检测线程（只负责检测，不负责上报）
	if (taskLocating_)
	{
		std::cout << "[ThreadManager] 停止热成像检测线程..." << std::endl;
		taskLocating_->stop();
	}

	// 3. 停止RTSP推流线程（数据输出）
	if (taskRTSPStream_)
	{
		std::cout << "[ThreadManager] 停止RTSP推流线程..." << std::endl;
		taskRTSPStream_->stop();
	}

	// 4. 停止目标追踪线程（可见光视频处理）
	if (taskObjectTracking_)
	{
		std::cout << "[ThreadManager] 停止目标追踪线程..." << std::endl;
		taskObjectTracking_->stop();
	}

	// 5. 停止显示处理线程（数据处理）
	if (taskDisplay_)
	{
		std::cout << "[ThreadManager] 停止显示处理线程..." << std::endl;
		taskDisplay_->stop();
	}

	// 6. 停止热成像数据捕获线程（温度数据源）
	if (taskThermal_)
	{
		std::cout << "[ThreadManager] 停止热成像数据捕获线程..." << std::endl;
		taskThermal_->stop();
	}

	// 7. 停止视频捕获线程（数据源）- 包含海康SDK清理逻辑
	if (taskVideo_)
	{
		std::cout << "[ThreadManager] 停止视频捕获线程..." << std::endl;
		taskVideo_->stop();
	}

	std::cout << "[ThreadManager] 所有任务线程停止完成" << std::endl;
}