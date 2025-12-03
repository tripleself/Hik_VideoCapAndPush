/**
 * @file thermometry_test_example.cpp
 * @brief 实时测温功能测试示例
 *
 * 这个示例展示了如何使用TaskVideoCapture的新增实时测温功能：
 * 1. 从海康热成像相机获取最高温、最低温数据
 * 2. 实时显示温度信息
 * 3. 将温度数据存储到SharedData中供其他模块使用
 *
 * 编译命令示例：
 * g++ -std=c++11 thermometry_test_example.cpp -I../head -I../external/CH-HCNetSDKV6.1.9.48/include -o thermometry_test
 */

#include <iostream>
#include <thread>
#include <chrono>
#include <nlohmann/json.hpp>
#include "TaskVideoCapture.h"
#include "SharedData.h"

// 示例：如何读取并显示实时温度数据
void displayTemperatureData(const SharedData &data, int deviceIndex)
{
    if (deviceIndex == 0)
    {
        std::lock_guard<std::mutex> lock(data.realtimeTemp_mutex_1);
        const auto &tempData = data.realtimeTemp_1;

        if (tempData.isValid)
        {
            std::cout << "=== 设备1 温度数据 ===" << std::endl;
            std::cout << "最高温度: " << tempData.highestTemperature << "°C" << std::endl;
            std::cout << "最低温度: " << tempData.lowestTemperature << "°C" << std::endl;
            std::cout << "中心温度: " << tempData.centerTemperature << "°C" << std::endl;
            std::cout << "规则名称: " << tempData.ruleName << std::endl;
            std::cout << "通道号: " << tempData.channelNo << std::endl;
            std::cout << "相对时标: " << tempData.relativeTimeStr << " (带时区)" << std::endl;
            std::cout << "绝对时标: " << tempData.absoluteTimeStr << " (UTC)" << std::endl;
            std::cout << "原始时间戳: " << tempData.timestamp << std::endl;
            std::cout << "========================" << std::endl;
        }
        else
        {
            std::cout << "设备1 温度数据无效或未获取到数据" << std::endl;
        }
    }
    else if (deviceIndex == 1)
    {
        std::lock_guard<std::mutex> lock(data.realtimeTemp_mutex_2);
        const auto &tempData = data.realtimeTemp_2;

        if (tempData.isValid)
        {
            std::cout << "=== 设备2 温度数据 ===" << std::endl;
            std::cout << "最高温度: " << tempData.highestTemperature << "°C" << std::endl;
            std::cout << "最低温度: " << tempData.lowestTemperature << "°C" << std::endl;
            std::cout << "中心温度: " << tempData.centerTemperature << "°C" << std::endl;
            std::cout << "规则名称: " << tempData.ruleName << std::endl;
            std::cout << "通道号: " << tempData.channelNo << std::endl;
            std::cout << "相对时标: " << tempData.relativeTimeStr << " (带时区)" << std::endl;
            std::cout << "绝对时标: " << tempData.absoluteTimeStr << " (UTC)" << std::endl;
            std::cout << "原始时间戳: " << tempData.timestamp << std::endl;
            std::cout << "========================" << std::endl;
        }
        else
        {
            std::cout << "设备2 温度数据无效或未获取到数据" << std::endl;
        }
    }
}

// 示例：如何检查温度阈值并进行报警
bool checkTemperatureAlarm(const RealTimeTemperatureData &tempData, float alarmThreshold)
{
    if (!tempData.isValid)
    {
        return false;
    }

    if (tempData.highestTemperature > alarmThreshold)
    {
        std::cout << "🚨 温度报警！最高温度 " << tempData.highestTemperature
                  << "°C 超过阈值 " << alarmThreshold << "°C" << std::endl;
        return true;
    }

    return false;
}

int main()
{
    std::cout << "======================================" << std::endl;
    std::cout << "海康威视实时测温功能测试示例" << std::endl;
    std::cout << "======================================" << std::endl;

    // 创建共享数据对象
    SharedData sharedData;
    sharedData.isRunning = true;

    // 配置设备信息 - 请根据实际设备修改
    std::vector<nlohmann::json> deviceConfigs;
    nlohmann::json device1Config;
    device1Config["ip"] = "192.168.1.100"; // 修改为实际设备IP
    device1Config["port"] = 8000;          // 修改为实际端口
    device1Config["username"] = "admin";   // 修改为实际用户名
    device1Config["password"] = "12345";   // 修改为实际密码
    deviceConfigs.push_back(device1Config);

    // 如果有第二个设备，可以添加
    /*
    nlohmann::json device2Config;
    device2Config["ip"] = "192.168.1.101";
    device2Config["port"] = 8000;
    device2Config["username"] = "admin";
    device2Config["password"] = "12345";
    deviceConfigs.push_back(device2Config);
    */

    // 创建视频捕获任务（包含实时测温功能）
    int cameraCount = 1; // 或 2，根据实际设备数量
    TaskVideoCapture videoCapture(cameraCount, deviceConfigs, sharedData);

    std::cout << "启动视频捕获和实时测温..." << std::endl;
    videoCapture.start();

    // 等待一段时间让系统初始化
    std::this_thread::sleep_for(std::chrono::seconds(3));

    // 温度报警阈值
    float alarmThreshold = 40.0f; // 40°C

    // 主循环：周期性检查温度数据
    int loopCount = 0;
    while (sharedData.isRunning && loopCount < 100)
    { // 运行100次循环后退出

        // 显示设备1的温度数据
        std::cout << "\n--- 循环 " << (loopCount + 1) << " ---" << std::endl;
        displayTemperatureData(sharedData, 0);

        // 检查设备1温度报警
        {
            std::lock_guard<std::mutex> lock(sharedData.realtimeTemp_mutex_1);
            checkTemperatureAlarm(sharedData.realtimeTemp_1, alarmThreshold);
        }

        // 如果有第二个设备，也显示其温度数据
        if (cameraCount >= 2)
        {
            displayTemperatureData(sharedData, 1);

            std::lock_guard<std::mutex> lock(sharedData.realtimeTemp_mutex_2);
            checkTemperatureAlarm(sharedData.realtimeTemp_2, alarmThreshold);
        }

        // 等待5秒再检查下一次
        std::this_thread::sleep_for(std::chrono::seconds(5));
        loopCount++;
    }

    std::cout << "\n测试结束，正在停止系统..." << std::endl;
    sharedData.isRunning = false;
    videoCapture.stop();

    std::cout << "系统已安全停止。" << std::endl;
    return 0;
}

/**
 * 使用说明：
 *
 * 1. 实时测温数据存储：
 *    - 设备1的温度数据存储在 sharedData.realtimeTemp_1
 *    - 设备2的温度数据存储在 sharedData.realtimeTemp_2
 *    - 每个设备都有对应的互斥锁保证线程安全
 *
 * 2. 温度数据结构 RealTimeTemperatureData 包含：
 *    - highestTemperature: 最高温度
 *    - lowestTemperature: 最低温度
 *    - centerTemperature: 中心点温度
 *    - isValid: 数据有效性标志
 *    - ruleName: 测温规则名称
 *    - ruleID: 规则ID
 *    - timestamp: 原始时间戳
 *    - channelNo: 通道号
 *    - relativeTimeStr: 相对时标字符串 (带时区，如东八区时间)
 *    - absoluteTimeStr: 绝对时标字符串 (UTC时间)
 *
 * 3. 其他模块使用温度数据的方法：
 *    ```cpp
 *    // 读取设备1的温度数据
 *    {
 *        std::lock_guard<std::mutex> lock(sharedData.realtimeTemp_mutex_1);
 *        if (sharedData.realtimeTemp_1.isValid) {
 *            float maxTemp = sharedData.realtimeTemp_1.highestTemperature;
 *            float minTemp = sharedData.realtimeTemp_1.lowestTemperature;
 *            std::string timeWithTimezone = sharedData.realtimeTemp_1.relativeTimeStr;
 *            std::string utcTime = sharedData.realtimeTemp_1.absoluteTimeStr;
 *            // 处理温度数据和时间信息...
 *        }
 *    }
 *    ```
 *
 * 4. 注意事项：
 *    - 确保设备支持热成像测温功能
 *    - 确保设备已正确配置测温规则
 *    - 实时测温功能会在设备登录成功后自动启动
 *    - 温度数据通过回调函数异步更新
 *    - 程序退出时会自动停止实时测温功能
 *    - 时间戳解析：
 *      * relativeTimeStr: 相对时标，带时区信息（如东八区时间）
 *      * absoluteTimeStr: 绝对时标，UTC时间，不带时区
 *      * 两者的时间差通常为时区偏移量（如东八区相差8小时）
 */