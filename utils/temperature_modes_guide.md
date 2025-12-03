# 海康热成像相机测温模式说明

## 📊 测温模式类型

根据海康威视SDK，热成像相机支持三种测温模式：

### 1. 点测温 (byRuleCalibType = 0)
- **特点**: 测量单个点的温度
- **数据获取**: `struPointThermCfg.fTemperature`
- **适用场景**: 精确测量特定位置温度
- **返回数据**: 一个温度值

### 2. 框测温 (byRuleCalibType = 1) 
- **特点**: 测量矩形区域内的温度分布
- **数据获取**: `struLinePolygonThermCfg`
- **适用场景**: 监控区域温度范围
- **返回数据**: 最高温、最低温、平均温、温差

### 3. 线测温 (byRuleCalibType = 2)
- **特点**: 测量线段上的温度分布  
- **数据获取**: `struLinePolygonThermCfg`
- **适用场景**: 监控线性目标温度
- **返回数据**: 最高温、最低温、平均温、温差

## 🔧 在普通模式下获取温度数据

您**不需要启动专家模式**就可以获取温度数据！只需要：

### 方法1: 使用设备Web界面预配置测温规则
1. 通过浏览器访问设备IP (如: http://192.168.1.100)
2. 登录设备管理界面
3. 进入"配置" → "事件" → "智能事件" → "测温"
4. 添加测温规则（点、线或框）
5. 保存配置

### 方法2: 让我们的程序自动获取所有已配置的规则
- 设置 `byRuleID = 0` (获取全部规则)
- 程序会自动识别并处理所有类型的测温规则

## 📝 代码中的处理逻辑

```cpp
// 我们的回调函数会自动识别测温类型
if (thermData->byRuleCalibType == 0) {
    // 点测温: 使用单一温度值
    float temp = thermData->struPointThermCfg.fTemperature;
    tempData.highestTemperature = temp;
    tempData.lowestTemperature = temp;
}
else if (thermData->byRuleCalibType == 1 || thermData->byRuleCalibType == 2) {
    // 框/线测温: 直接获取最高最低温
    tempData.highestTemperature = thermData->struLinePolygonThermCfg.fMaxTemperature;
    tempData.lowestTemperature = thermData->struLinePolygonThermCfg.fMinTemperature;
}
```

## 🚀 快速上手指南

### Step 1: 配置设备测温规则
在设备Web界面中至少配置一个测温规则：
- **推荐**: 配置一个全屏框测温规则，这样可以获取整个画面的最高温和最低温

### Step 2: 运行程序
程序启动后会自动：
1. 登录设备
2. 启动实时测温 (`byRuleID = 0` 获取所有规则)
3. 在回调函数中解析温度数据
4. 存储到 SharedData 中

### Step 3: 读取温度数据
```cpp
{
    std::lock_guard<std::mutex> lock(sharedData.realtimeTemp_mutex_1);
    if (sharedData.realtimeTemp_1.isValid) {
        float maxTemp = sharedData.realtimeTemp_1.highestTemperature;
        float minTemp = sharedData.realtimeTemp_1.lowestTemperature;
        std::cout << "最高温: " << maxTemp << "°C, 最低温: " << minTemp << "°C" << std::endl;
    }
}
```

## 🐛 问题排查

### 温度显示 -100°C 的原因：
1. **设备未配置测温规则** - 在Web界面配置至少一个测温规则
2. **测温功能未启用** - 检查设备测温功能是否开启
3. **通道号错误** - 确保使用正确的热成像通道号（通常是2）
4. **数据解析错误** - 我们已经修复了解析逻辑

### 检查日志信息：
运行程序后查看日志，应该显示：
```
[TaskVideoCapture] 设备 1 框测温数据 - 最高温: 25.3°C, 最低温: 23.1°C, 平均温: 24.2°C
```

## 💡 建议配置

为了获取最全面的温度信息，建议在设备Web界面配置：
1. **一个全屏框测温规则** - 获取整个画面的温度范围
2. **可选：关键区域的点测温** - 监控特定位置

这样既能获取整体的最高最低温，又能监控重点区域的精确温度。 