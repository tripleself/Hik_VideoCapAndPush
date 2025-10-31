# YOLO TensorRT 目标检测与追踪工具

这是一个基于TensorRT的YOLOv11目标检测与ByteTrack追踪的C++项目。本项目支持对图像和视频文件进行高性能目标检测，并集成了基于ByteTrack算法的多目标追踪功能。

## 功能特点

### 检测功能
- 支持对单张图片、图片目录和视频进行目标检测
- 自动识别输入文件类型（图片/视频）
- 支持选择不同的精度模式（FP32/FP16/INT8）
- 支持自定义置信度和NMS阈值
- 自动使用默认模型文件（除非显式指定）
- 检测结果可视化并保存

### 追踪功能
- 基于ByteTrack算法的单类别目标追踪
- 支持自定义追踪类别（默认：人员）
- 可选的ReID特征支持
- 可调整的追踪缓冲区大小
- 实时性能统计显示（检测时间、追踪时间分离统计）
- 目标状态标识（新目标[NEW]、丢失目标[LOST]）
- 基于类别的固定颜色显示

### 虚拟检测线计数模块 (v2.2)
- **模块化设计**：将计数功能完全封装在 `CountingLineModule` 类中，作为独立的静态库 `counting_line.lib` (Windows) 提供。
- **接口驱动**：通过简洁的C++接口进行初始化、更新和结果获取，易于集成到任何项目中。
- **双向计数**：智能检测目标穿越虚拟检测线（支持从上到下和从下到上）。
- **唯一性保证**：每个目标仅计数一次，避免在视频中重复计数。
- **详细数据记录**：自动生成包含四项关键信息的 `txt` 文件：
  - `Target_ID`：从1开始的顺序ID，用于唯一标识穿越事件。
  - `real_processing_time_ms`：单帧的真实处理耗时。
  - `Current_Frame_Time_ms`：当前帧在视频中的时间戳。
  - `Real_Time_ms`：扣除处理延迟的目标穿越真实时间。
- **可视化支持**：提供在视频帧上绘制检测线和标签的功能。



### 4. 添加源文件

将以下文件添加到项目中：
- include目录下的所有头文件
- src目录下的所有源文件
- main.cpp
- 将yolo11n.engine复制到项目生成目录

#### 3. VS2022编译（Windows）
按照原有VS2022配置说明，另外需要添加：
- ByteTrack库的包含目录
- Eigen3库的包含目录
- 链接ByteTrack静态库

## 工作流程

### 1. 检测流程
检测流程保持原有设计：
1. **模型加载** - 加载TensorRT引擎文件
2. **图像预处理** - 缩放、Letterbox填充、格式转换、归一化
3. **模型推理** - TensorRT执行推理
4. **后处理** - 转置、解码、NMS、坐标变换
5. **结果绘制** - 可视化检测结果

### 2. 追踪流程（新增）
1. **追踪器初始化** - 配置ByteTrack参数、指定追踪类别
2. **检测结果过滤** - 只保留指定类别的检测结果
3. **追踪更新** - 使用ByteTrack算法更新目标轨迹
4. **结果转换** - 将追踪结果转换为标准格式
5. **可视化绘制** - 绘制追踪ID、类别、状态信息

### 3. 计数流程 (使用 `CountingLineModule`)
1. **模块初始化** - 使用视频参数（宽、高、FPS）创建 `CountingLineModule` 实例。
2. **文件配置** - 设置计数结果的输出文件路径。
3. **计数更新** - 在视频处理循环中，将追踪结果传入 `updateCounting` 方法。
4. **结果获取** - 随时通过 `getTotalCount` 获取当前总数。
5. **结束计数** - 视频处理结束后，调用 `finishCounting` 保存统计信息。

## 如何使用 `CountingLineModule` (C++示例)

`CountingLineModule` 被设计为一个独立的组件，您可以轻松地在自己的代码中使用它。

```cpp
#include "counting_line.h"
#include <opencv2/opencv.hpp>

void countingExample(const std::string& videoPath) {
    // 1. 初始化视频和获取参数
    cv::VideoCapture cap(videoPath);
    int frame_width = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_WIDTH));
    int frame_height = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_HEIGHT));
    double fps = cap.get(cv::CAP_PROP_FPS);

    // 2. 创建计数模块实例
    CountingLineModule counter(frame_width, frame_height, fps, videoPath);
    
    // 3. 设置计数文件并开始
    counter.setCountingFile("my_results.txt");
    counter.startCounting();

    cv::Mat frame;
    int frame_count = 0;
    while (cap.read(frame)) {
        frame_count++;
        
        // 假设您已经从追踪器获得了追踪结果 (std::vector<TrackResult> tracks)
        std::vector<TrackResult> tracks; // 此处应为您的追踪结果
        
        // 4. 更新计数
        double current_frame_time_ms = (frame_count - 1) * (1000.0 / fps);
        double processing_time_ms = 25.0; // 假设处理时间
        counter.updateCounting(tracks, current_frame_time_ms, processing_time_ms);
        
        // 5. 在帧上绘制检测线
        counter.drawDetectionLine(frame);
        
        // 显示或保存帧
        cv::imshow("Counting Example", frame);
        if (cv::waitKey(1) == 'q') break;
    }
    
    // 6. 完成计数并保存文件
    counter.finishCounting(frame_count);
    
    std::cout << "Total objects crossed: " << counter.getTotalCount() << std::endl;
}
```

### COCO数据集类别ID对照表

常用追踪类别：
- 0: person (人)
- 1: bicycle (自行车)  
- 2: car (汽车)
- 3: motorcycle (摩托车)
- 5: bus (公交车)
- 7: truck (卡车)

## 核心技术实现

### 1. TrackerModule类
```cpp
// 追踪结果结构
struct TrackResult {
    int track_id;      // 追踪ID
    float bbox[4];     // 边界框坐标 (x1, y1, x2, y2)
    float conf;        // 置信度
    int classId;       // 类别ID
    bool is_new;       // 是否为新目标
    bool is_lost;      // 是否丢失目标
};

// 追踪模块类
class TrackerModule {
public:
    TrackerModule(int frame_rate, int track_buffer, bool use_reid, int track_class);
    std::vector<TrackResult> update(const std::vector<Detection> &detections);
    void setTrackClass(int class_id);
    static void drawTrackResults(cv::Mat &img, const std::vector<TrackResult> &track_results);
    static cv::Scalar getClassColor(int class_id);
};
```

### 2. 模块化设计
- **完全模块化** - 追踪功能独立封装，不影响原有检测功能
- **向后兼容** - 原有API接口完全保持不变
- **可选启用** - 通过命令行参数控制追踪功能开关

### 3. 性能优化
- **分离统计** - 检测时间和追踪时间独立计算
- **单类别追踪** - 避免多类别混合追踪的复杂性
- **固定颜色** - 基于类别的固定颜色映射，视觉一致性好

## 在VS2022中集成和使用该项目

### 方法一：使用预编译的静态库（推荐）

#### 1. 新建VS2022项目

1. 打开Visual Studio 2022
2. 选择"创建新项目"
3. 选择"控制台应用" (C++)
4. 设置项目名称（如：YoloTracker）和位置
5. 确保平台配置为x64，点击"创建"

#### 2. 复制必要文件到项目目录

将以下文件和文件夹复制到您的项目根目录：
```
YoloTracker/
├── include/           # 复制完整的include文件夹
├── lib/               # 创建lib文件夹
│   ├── counting_line.lib # 从原项目build目录复制
│   ├── yolo_infer.lib # 从原项目build目录复制
│   └── bytetrack.lib  # 从原项目build目录复制
├── models/            # 创建models文件夹
│   └── yolo11n.plan   # 复制TensorRT引擎文件
├── test_video.mp4     # 复制测试视频
├── test_image.jpg     # 复制测试图像
└── example_main.cpp   # 复制示例代码文件（可选）
```

#### 3. 配置项目属性

右键项目 → 属性，确保配置为x64平台：

##### 3.1 C/C++ → 常规 → 附加包含目录
```
$(ProjectDir)include
C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.6\include
D:\Program\TensorRT-8.6.1.6\include
D:\Program\opencv\opencv\build\include
C:\Program Files (x86)\Eigen3\include\eigen3
```

##### 3.2 链接器 → 常规 → 附加库目录
```
$(ProjectDir)lib
C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.6\lib\x64
D:\Program\TensorRT-8.6.1.6\lib
D:\Program\opencv\opencv\build\x64\vc16\lib
```

##### 3.3 链接器 → 输入 → 附加依赖项
```
yolo_infer.lib
bytetrack.lib
counting_line.lib
nvinfer.lib
cudart.lib
nvonnxparser.lib
opencv_world4110.lib   # Release模式
opencv_world4110d.lib  # Debug模式（根据您的OpenCV版本调整）
```

##### 3.4 C/C++ → 预处理器 → 预处理器定义
```
NOMINMAX
API_EXPORTS
```

#### 4. 示例代码


### 集成追踪功能到您的项目

1. **包含头文件**：
```cpp
#include "infer.h"
#include "tracker.h"
```

2. **创建检测器和追踪器**：
```cpp
YoloDetector detector("model.engine", 0, 0.45f, 0.25f, 80, true, false);
TrackerModule tracker(30, 30, false, 0);  // 30fps, 30帧缓冲, 不使用ReID, 追踪人员
```

3. **执行检测和追踪**：
```cpp
cv::Mat frame;
std::vector<Detection> detections = detector.inference(frame);
std::vector<TrackResult> tracks = tracker.update(detections);
```

4. **绘制追踪结果**：
```cpp
TrackerModule::drawTrackResults(frame, tracks);
```

### 高级定制选项

1. **自定义追踪类别**：
```cpp
tracker.setTrackClass(2);  // 切换到追踪汽车
```

2. **处理追踪结果**：
```cpp
for (const auto& track : tracks) {
    if (track.is_new) {
        std::cout << "New track detected: ID " << track.track_id << std::endl;
    }
    if (track.is_lost) {
        std::cout << "Track lost: ID " << track.track_id << std::endl;
    }
}
```

## 性能优化提示

1. **追踪参数调优**：
   - 选择合适的追踪类别减少计算负担
   - 根据视频帧率调整track_buffer大小
   - 必要时关闭ReID功能提升性能

2. **硬件建议**：
   - GPU显存 >= 4GB
   - CUDA计算能力 >= 6.1

## 故障排除

### 常见问题

1. **编译错误**
   - 检查ByteTrack库路径配置
   - 确认Eigen3库版本兼容性
   - 验证所有依赖库路径正确

2. **运行时错误**
   - 确保GPU驱动和CUDA版本兼容
   - 检查TensorRT引擎文件完整性
   - 验证追踪类别ID在有效范围内

3. **追踪效果问题**
   - 调整置信度阈值（影响检测质量）
   - 修改追踪缓冲区大小（影响目标保持时间）
   - 检查追踪类别设置是否正确

4. **性能问题**
   - 考虑降低输入分辨率
   - 关闭不必要的ReID功能
   - 使用更快的精度模式

### 日志信息
程序提供详细的运行日志：
- 模型和追踪器初始化状态
- 实时处理性能统计
- 追踪参数配置信息
- 最终结果统计

## 更新记录

### v2.2 虚拟检测线计数功能 (最新)
- ✅ 新增虚拟检测线计数功能
- ✅ 智能目标穿越检测（双向计数支持）
- ✅ 详细的计数数据记录（txt格式）
- ✅ 四项关键信息记录：目标ID、检测时间、当前帧时间、真实时间
- ✅ 可视化检测线和实时计数显示
- ✅ 防重复计数机制
- ✅ 更新示例代码支持计数功能

### v2.1 VS2022集成支持
- ✅ 完整的VS2022项目集成指导
- ✅ 提供三种使用示例代码：
  - 简单目标检测示例
  - 视频目标追踪示例（现已包含计数功能）
  - 实时摄像头追踪示例
- ✅ 支持预编译库和源码编译两种方式
- ✅ 详细的环境配置和故障排除指南
- ✅ 完善的API接口封装
- ✅ 模块化设计，易于集成到其他项目

### v2.0 追踪功能集成
- ✅ 集成ByteTrack算法
- ✅ 实现单类别追踪（解决多类别混乱问题）
- ✅ 基于类别的固定颜色系统
- ✅ 完整的命令行接口
- ✅ 模块化设计，向后兼容
- ✅ 详细的性能统计和状态显示

### 技术改进
- 修复了颜色设置基于track_id的不合理设计
- 解决了classId永远为0的问题  
- 简化了多类别追踪为单类别追踪
- 优化了追踪结果的视觉显示效果
- 提供了完整的VS2022开发环境集成方案

## 联系支持

如有任何问题或需要更多帮助，请联系项目维护者。

---

**注意**: 追踪功能仅支持视频文件，图像和目录处理会自动降级为检测模式。首次使用请确保TensorRT引擎文件和ByteTrack库已正确配置。

