#pragma once
#include "SharedData.h"
#include <thread>
#include <opencv2/opencv.hpp>

class TaskDisplay
{
public:
	TaskDisplay(SharedData &data, bool enableDisplay = false); // 构造函数，初始化显示控制标志
	~TaskDisplay();											   // 析构函数，确保线程安全停止
	void start();											   // 启动显示线程
	void stop();											   // 停止显示线程
	void setDisplayEnabled(bool enabled);					   // 设置是否启用窗口显示
	bool isDisplayEnabled() const;							   // 获取显示状态
	static float g_alarmThreshold;							   // 报警阈值

private:
	void run();																// 线程主函数
	void processVideoFrames();												// 处理视频帧和温度数据
	void updateDisplay(const cv::Mat &displayFrame);						// 更新窗口显示
	void initializeDisplay();												// 初始化显示窗口
	void cleanupDisplay();													// 清理显示窗口
	void processTemperatureData(const cv::Mat &tempMatrix, cv::Mat &frame); // 处理温度数据

	SharedData &data_; // 共享数据引用
	// cv::VideoCapture& cap_; // 视频捕获对象引用
	std::thread thread_;	 // 视频捕获对象线程
	bool enableDisplay_;	 // 控制是否启用窗口显示
	bool windowInitialized_; // 窗口是否已初始化
	std::string windowName_; // 窗口名称
};