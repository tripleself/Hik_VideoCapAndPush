#include "TaskThermalCapture.h"
#include <iostream>
#include <vector>
#include <algorithm>
#include <chrono>
#include <cmath>

// 构造函数：初始化设备用户ID和共享数据
TaskThermalCapture::TaskThermalCapture(const std::vector<LONG> &userIDs, SharedData &data)
	: userIDs_(userIDs), data_(data)
{
	std::cout << "[TaskThermalCapture] 初始化基于颜色分析的温度数据捕获任务，设备数量: " << userIDs_.size() << std::endl;

	// 初始化缓存数据
	cachedMinTemp_1_ = 20.0f;
	cachedMaxTemp_1_ = 60.0f;
	cachedMinTemp_2_ = 20.0f;
	cachedMaxTemp_2_ = 60.0f;
	lastTempUpdateTime_1_ = std::chrono::steady_clock::now();
	lastTempUpdateTime_2_ = std::chrono::steady_clock::now();
	frameCount_ = 0;
	totalProcessingTime_ = 0;

	// 初始化调色板缓存
	paletteInitialized_ = false;
	thresholdGrayValue_ = 128.0f; // 默认阈值
	percentileThreshold_ = 0.8f;  // 默认80%分位数
}

// 析构函数：确保线程安全退出
TaskThermalCapture::~TaskThermalCapture()
{
	stop();
}

// 启动热成像数据捕获线程
void TaskThermalCapture::start()
{
	std::cout << "[TaskThermalCapture] 启动基于颜色分析的温度数据捕获线程..." << std::endl;
	thread_ = std::thread(&TaskThermalCapture::run, this);
}

// 停止热成像数据捕获线程，确保线程安全
void TaskThermalCapture::stop()
{
	data_.isRunning = false;
	std::cout << "[TaskThermalCapture] 温度数据捕获线程正在退出..." << std::endl;
	if (thread_.joinable())
	{
		thread_.join();
	}

	// 输出性能统计
	if (frameCount_ > 0)
	{
		double avgProcessingTime = totalProcessingTime_ / frameCount_;
		std::cout << "[TaskThermalCapture] 性能统计 - 总帧数: " << frameCount_
				  << ", 平均处理时间: " << avgProcessingTime << " ms/帧" << std::endl;
	}

	std::cout << "[TaskThermalCapture] 温度数据捕获线程已安全退出" << std::endl;
}

/**
 * @brief 设置百分位数阈值
 * @param percentile 百分位数值 (0.0-1.0, 如0.8表示80%分位数)
 * @return 是否设置成功
 */
bool TaskThermalCapture::setPercentileThreshold(float percentile)
{
	// 参数验证
	if (percentile < 0.0f || percentile > 1.0f)
	{
		std::cerr << "[TaskThermalCapture] 错误：百分位数必须在0.0-1.0范围内，当前值: " << percentile << std::endl;
		return false;
	}

	percentileThreshold_ = percentile;
	paletteInitialized_ = false; // 重置初始化标志，强制重新计算阈值

	std::cout << "[TaskThermalCapture] 百分位数阈值已更新为: " << (percentile * 100) << "%" << std::endl;
	return true;
}

/**
 * @brief 获取缓存的温度范围数据
 * @param deviceIdx 设备索引（0或1）
 * @param minTemp 输出最低温度
 * @param maxTemp 输出最高温度
 * @return 是否获取到有效数据
 */
bool TaskThermalCapture::getCachedTemperatureRange(int deviceIdx, float &minTemp, float &maxTemp)
{
	auto currentTime = std::chrono::steady_clock::now();
	const auto cacheTimeout = std::chrono::milliseconds(500); // 500ms缓存超时

	if (deviceIdx == 0)
	{
		// 尝试获取新的温度数据
		{
			std::lock_guard<std::mutex> lock(data_.realtimeTemp_mutex_1);
			if (data_.realtimeTemp_1.isValid)
			{
				cachedMinTemp_1_ = data_.realtimeTemp_1.lowestTemperature;
				cachedMaxTemp_1_ = data_.realtimeTemp_1.highestTemperature;
				lastTempUpdateTime_1_ = currentTime;
			}
		}

		// 检查缓存是否有效
		auto timeSinceUpdate = currentTime - lastTempUpdateTime_1_;
		if (timeSinceUpdate < cacheTimeout)
		{
			minTemp = cachedMinTemp_1_;
			maxTemp = cachedMaxTemp_1_;
			return true;
		}
	}
	else if (deviceIdx == 1)
	{
		// 尝试获取新的温度数据
		{
			std::lock_guard<std::mutex> lock(data_.realtimeTemp_mutex_2);
			if (data_.realtimeTemp_2.isValid)
			{
				cachedMinTemp_2_ = data_.realtimeTemp_2.lowestTemperature;
				cachedMaxTemp_2_ = data_.realtimeTemp_2.highestTemperature;
				lastTempUpdateTime_2_ = currentTime;
			}
		}

		// 检查缓存是否有效
		auto timeSinceUpdate = currentTime - lastTempUpdateTime_2_;
		if (timeSinceUpdate < cacheTimeout)
		{
			minTemp = cachedMinTemp_2_;
			maxTemp = cachedMaxTemp_2_;
			return true;
		}
	}

	return false;
}

/**
 * @brief 计算向量的指定百分位数值
 * @param values 输入数值向量
 * @param percentile 百分位数 (0.0-1.0)
 * @return 百分位数对应的值
 */
float TaskThermalCapture::calculatePercentile(std::vector<float> &values, float percentile)
{
	if (values.empty())
	{
		return 0.0f;
	}

	// 排序向量
	std::sort(values.begin(), values.end());

	// 计算百分位数对应的索引
	float index = percentile * (values.size() - 1);
	int lowerIndex = static_cast<int>(floor(index));
	int upperIndex = static_cast<int>(ceil(index));

	// 边界检查
	lowerIndex = (std::max)(0, (std::min)(lowerIndex, static_cast<int>(values.size() - 1)));
	upperIndex = (std::max)(0, (std::min)(upperIndex, static_cast<int>(values.size() - 1)));

	// 线性插值
	if (lowerIndex == upperIndex)
	{
		return values[lowerIndex];
	}
	else
	{
		float weight = index - lowerIndex;
		return values[lowerIndex] * (1.0f - weight) + values[upperIndex] * weight;
	}
}

/**
 * @brief 一次性初始化调色板并计算阈值灰度值
 * @param frame 热成像视频帧
 * @return 是否初始化成功
 */
bool TaskThermalCapture::initializePalette(const cv::Mat &frame)
{
	if (paletteInitialized_)
	{
		return true; // 已经初始化过了
	}

	// 温度条区域：左上角(1242,101)，宽35像素，高517像素
	const int palette_x = 1242;
	const int palette_y = 101;
	const int palette_width = 35;
	const int palette_height = 517;

	// 检查边界
	if (palette_x + palette_width > frame.cols || palette_y + palette_height > frame.rows)
	{
		std::cerr << "[TaskThermalCapture] 初始化失败：温度条区域超出图像边界" << std::endl;
		return false;
	}

	// 提取温度条区域
	cv::Mat paletteROI = frame(cv::Rect(palette_x, palette_y, palette_width, palette_height));

	// 转为灰度图像
	cv::Mat grayPalette;
	if (paletteROI.channels() == 3)
	{
		cv::cvtColor(paletteROI, grayPalette, cv::COLOR_BGR2GRAY);
	}
	else
	{
		grayPalette = paletteROI.clone();
	}

	// 收集所有像素值用于百分位数计算
	std::vector<float> pixelValues;
	pixelValues.reserve(palette_width * palette_height);

	for (int y = 0; y < grayPalette.rows; y++)
	{
		for (int x = 0; x < grayPalette.cols; x++)
		{
			pixelValues.push_back(static_cast<float>(grayPalette.at<uchar>(y, x)));
		}
	}

	// 计算不同百分位数值用于参考
	float percentile70 = calculatePercentile(pixelValues, 0.7f);
	float percentile80 = calculatePercentile(pixelValues, 0.8f);
	float percentile90 = calculatePercentile(pixelValues, 0.9f);

	// 使用指定百分位数作为阈值
	thresholdGrayValue_ = calculatePercentile(pixelValues, percentileThreshold_);

	paletteInitialized_ = true;
	// std::cout << "[TaskThermalCapture] 调色板初始化成功，百分位数阈值分析:" << std::endl;
	// std::cout << "  - 70%分位数: " << percentile70 << " (适中敏感度)" << std::endl;
	// std::cout << "  - 80%分位数: " << percentile80 << " (推荐默认)" << std::endl;
	// std::cout << "  - 90%分位数: " << percentile90 << " (严格检测)" << std::endl;
	// std::cout << "  - 当前使用: " << thresholdGrayValue_ << " (" << (percentileThreshold_ * 100) << "%分位数)" << std::endl;

	return true;
}

/**
 * @brief 从温度条区域读取调色板颜色范围（已简化）
 * @param frame 热成像视频帧
 * @return 调色板的灰度值向量，从低温到高温排列
 */
std::vector<float> TaskThermalCapture::extractTemperaturePalette(const cv::Mat &frame)
{
	// 已简化：不再提取复杂调色板，直接返回空向量
	// 调色板初始化由initializePalette()函数处理
	return std::vector<float>();
}

/**
 * @brief 创建屏蔽区域掩码
 * @param frame 热成像视频帧
 * @return 屏蔽掩码（255=有效区域，0=屏蔽区域）
 */
cv::Mat TaskThermalCapture::createMaskRegions(const cv::Mat &frame)
{
	cv::Mat mask = cv::Mat::ones(frame.size(), CV_8UC1) * 255;

	// 屏蔽区域1：(1090,90) 到 (1235,145)
	cv::rectangle(mask, cv::Point(1090, 90), cv::Point(1235, 145), cv::Scalar(0), -1);

	// 屏蔽区域2：(1090,625) 到 (1235,670)
	cv::rectangle(mask, cv::Point(1090, 625), cv::Point(1235, 670), cv::Scalar(0), -1);

	// 屏蔽区域3：(1235,90) 到 (1280,625)
	cv::rectangle(mask, cv::Point(1235, 90), cv::Point(1280, 625), cv::Scalar(0), -1);

	return mask;
}

/**
 * @brief 极简化二值化温度映射（性能优化）
 * @param grayValue 像素灰度值（0-255）
 * @return 高温或低温值
 */
float TaskThermalCapture::mapGrayToTemperature(float grayValue, const std::vector<float> &palette,
											   float minTemp, float maxTemp)
{
	// 极简化：直接进行阈值比较，返回二值化温度
	return (grayValue > thresholdGrayValue_) ? HIGH_TEMP : LOW_TEMP;
}

/**
 * @brief 从热成像视频帧生成温度矩阵
 * @param frame 热成像视频帧（1280x720）
 * @param minTemp 最低温度
 * @param maxTemp 最高温度
 * @return 温度矩阵（640x512，CV_32FC1）
 */
cv::Mat TaskThermalCapture::generateTemperatureMatrix(const cv::Mat &frame, float minTemp, float maxTemp)
{
	if (frame.empty())
	{
		std::cerr << "[TaskThermalCapture] 输入视频帧为空" << std::endl;
		return cv::Mat();
	}

	// 创建屏蔽掩码
	cv::Mat mask = createMaskRegions(frame);

	// 转为灰度图像
	cv::Mat grayFrame;
	if (frame.channels() == 3)
	{
		cv::cvtColor(frame, grayFrame, cv::COLOR_BGR2GRAY);
	}
	else
	{
		grayFrame = frame.clone();
	}

	// 缩放到640x512
	cv::Mat resizedGray, resizedMask;
	cv::resize(grayFrame, resizedGray, cv::Size(640, 512));
	cv::resize(mask, resizedMask, cv::Size(640, 512));

	// 创建温度矩阵
	cv::Mat temperatureMatrix(512, 640, CV_32FC1);

	// 极简化像素遍历：直接阈值比较
	for (int y = 0; y < 512; y++)
	{
		for (int x = 0; x < 640; x++)
		{
			if (resizedMask.at<uchar>(y, x) == 0)
			{
				temperatureMatrix.at<float>(y, x) = LOW_TEMP; // 屏蔽区域为低温
			}
			else
			{
				float grayValue = static_cast<float>(resizedGray.at<uchar>(y, x));
				temperatureMatrix.at<float>(y, x) = (grayValue > thresholdGrayValue_) ? HIGH_TEMP : LOW_TEMP;
			}
		}
	}

	return temperatureMatrix;
}

// 线程主函数：循环从热成像视频流分析温度数据
void TaskThermalCapture::run()
{
	std::cout << "[TaskThermalCapture] 开始基于颜色分析的温度数据捕获循环..." << std::endl;

	while (data_.isRunning)
	{
		bool processedAnyFrame = false;

		// 检查热成像处理是否启用
		bool thermalProcessingEnabled = true;
		float environmentTempThreshold = 50.0f;
		{
			std::lock_guard<std::mutex> lock(data_.thermalProcessingConfigMutex);
			thermalProcessingEnabled = data_.thermalProcessingConfig.enableThermalProcessing;
			environmentTempThreshold = data_.thermalProcessingConfig.environmentTempThreshold;
		}

		// 如果热成像处理被禁用，跳过所有处理
		if (!thermalProcessingEnabled)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			continue;
		}

		// 处理设备1（一位端）
		if (userIDs_.size() > 0)
		{
			cv::Mat thermalFrame;
			float minTemp = 20.0f, maxTemp = 60.0f;

			// 获取热成像视频帧
			{
				std::lock_guard<std::mutex> lock(data_.thermal_mutex_1);
				if (!data_.thermal_video_frame_1.empty())
				{
					data_.thermal_video_frame_1.copyTo(thermalFrame);
				}
			}

			// 获取温度范围数据
			getCachedTemperatureRange(0, minTemp, maxTemp);

			// 检查环境最高温度是否达到阈值
			if (maxTemp < environmentTempThreshold)
			{
				// 环境温度过低，跳过处理
				static int skipCount = 0;
				if (++skipCount % 2000 == 0) // 每100次跳过输出一次日志
				{
					std::cout << "[TaskThermalCapture] 设备1环境温度过低(" << maxTemp << "°C < " << environmentTempThreshold << "°C)，跳过热成像处理" << std::endl;
				}
			}
			else
			{
				// 环境温度足够高，进行热成像处理
				if (!thermalFrame.empty())
				{
					if (!paletteInitialized_)
					{
						initializePalette(thermalFrame);
					}

					cv::Mat temperatureMatrix = generateTemperatureMatrix(thermalFrame, minTemp, maxTemp);

					if (!temperatureMatrix.empty())
					{
						std::lock_guard<std::mutex> lock(data_.thermalmatrix_mutex_1);
						temperatureMatrix.copyTo(data_.thermalMatrix_1);
						processedAnyFrame = true;
					}
				}
			}
		}

		// 处理设备2（二位端） - 固定处理两个摄像头
		if (userIDs_.size() > 1)
		{
			cv::Mat thermalFrame2;
			float minTemp2 = 20.0f, maxTemp2 = 60.0f;

			// 获取热成像视频帧
			{
				std::lock_guard<std::mutex> lock(data_.thermal_mutex_2);
				if (!data_.thermal_video_frame_2.empty())
				{
					data_.thermal_video_frame_2.copyTo(thermalFrame2);
				}
			}

			// 获取温度范围数据
			getCachedTemperatureRange(1, minTemp2, maxTemp2);

			// 检查环境最高温度是否达到阈值
			if (maxTemp2 < environmentTempThreshold)
			{
				// 环境温度过低，跳过处理
				static int skipCount2 = 0;
				if (++skipCount2 % 2000 == 0) // 每100次跳过输出一次日志
				{
					std::cout << "[TaskThermalCapture] 设备2环境温度过低(" << maxTemp2 << "°C < " << environmentTempThreshold << "°C)，跳过热成像处理" << std::endl;
				}
			}
			else
			{
				// 环境温度足够高，进行热成像处理
				if (!thermalFrame2.empty())
				{
					if (!paletteInitialized_)
					{
						initializePalette(thermalFrame2);
					}

					cv::Mat temperatureMatrix2 = generateTemperatureMatrix(thermalFrame2, minTemp2, maxTemp2);

					if (!temperatureMatrix2.empty())
					{
						std::lock_guard<std::mutex> lock(data_.thermalmatrix_mutex_2);
						temperatureMatrix2.copyTo(data_.thermalMatrix_2);
						processedAnyFrame = true;
					}
				}
			}
		}

		// 如果没有处理任何帧，短暂休眠避免CPU占用过高
		if (!processedAnyFrame)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}
	}

	std::cout << "[TaskThermalCapture] 基于颜色分析的温度数据捕获循环已退出" << std::endl;
}
