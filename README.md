# 海康热成像/可见光多路实时识别与推流系统

> 工控机后端（C++17）+ 双触控前端（TCP/RTSP）。两台海康相机（每台可见光+热成像）→ 多线程拉流/检测/融合 → 四路RTSP推流；串口(GYK)→CAN重编码→TCP广播；提供多前端一致控制。

## 功能特性
- 四路视频实时处理：两台相机的可见光与热成像各一路，共四路
- 可见光检测与追踪：TensorRT 加速 YOLO + ByteTrack + 虚拟计数线
- 热成像近似温度矩阵：基于温度条ROI分位数阈值 + 灰度二值化（CV_32FC1，640×512）
- 低延迟RTSP推流：FFmpeg API（H.264，ultrafast + zerolatency，禁B帧，时间戳帧控）+ CUDA BGR→YUV420P
- RS422(GYK) → CAN 48字节负载 + 4检测位 + CRC16 → TCP 广播至前端A/B
- 控制广播：前端发送 `CMD:SET_DIR:1/2`，后端向所有前端广播 `NOTIFY:SHOW_DIR:1/2`
- 原始录像（可见光）：海康SDK直存，自动切片与容量清理

## 目录结构
```
IPC_Code_cmake/
  head/                 # 头文件（任务/共享数据/配置）
  src/                  # 源码（多任务线程、控制与上报、推流等）
  external/             # 第三方（HCNetSDK、FFmpeg、dll等）
  rtspserver/           # rtsp-simple-server 可执行与配置
  utils/                # 工具与文档
  CMakeLists.txt        # 项目主 CMake
  config.json           # 运行时配置
  README.md             # 本文件
```

## 依赖与环境
- 操作系统：Windows 10/11（x64）
- 编译器与构建：MSVC 2019/2022，CMake
- 必要库：
  - OpenCV 4.x（图像处理与显示）
  - FFmpeg（external/ffmpeg 已提供二进制与头/库）
  - CUDA 11+（用于 BGR→YUV420P 转换加速）
  - 海康威视 SDK（external/CH-HCNetSDKV6.1.9.48）
  - nlohmann/json（external/nlohmann）
- RTSP 服务：`rtspserver/rtsp-simple-server.exe`

> 注意：HCNetSDK 与相关 DLL 需按厂商授权条款使用，确保可执行目录下能找到必要 DLL。

## 快速开始
### 1) 准备依赖
- 确认 `external/ffmpeg`、`external/CH-HCNetSDK...` 和必要 DLL 在可执行目录可见（如 `build/Release/`）。
- 安装 OpenCV（或在 CMake 配置中指向预编译包）。
- 确保安装 CUDA（可选，但建议启用以降低延迟）。

### 2) 配置 RTSP 服务器
```
cd rtspserver
start rtsp-simple-server.exe rtsp-simple-server.yml
```

### 3) 配置 config.json（关键片段）
```json
{
  "camera_count": 2,
  "hikvision_devices": [
    {"name":"一位端","ip":"192.168.1.15","port":8000,"username":"admin","password":"***"},
    {"name":"二位端","ip":"192.168.1.11","port":8000,"username":"admin","password":"***"}，
    "note": "一位端和二位端的热成像和可见光通道配置"
  ],
  "rtsp_server": {
    "exe_path": "rtspserver/rtsp-simple-server.exe",
    "config_path": "rtspserver/rtsp-simple-server.yml"
  },
  "stream_urls": {
    "local_ip1": "127.0.0.1",
    "local_ip2": "127.0.0.1",
    "rtsp_port": 8556,
    "note": "推流路径，使用单端口多路径方式，通过不同路径区分流"
  },
  "rtsp_streaming": {"resolution": {"width": 0, "height": 0}, "fps": 25},
  "video_save": {
    "enable_video_save": false,
    "video_save_path": "D:/RailwayVideos/",
    "max_file_size_mb": 1024,
    "max_storage_gb": 600,
    "cleanup_size_gb": 40
  },
  "thermal_processing": {
    "enable_thermal_processing": true,
    "environment_temp_threshold": 50.0
  }
}
```
### 3.1) 配置 tracking_config.json（片段）
```json
{
    "model": {
        "engine_path": "./models/20250928.plan",
        "gpu_id": 0,
        "num_class": 1,
        "note": "模型路径，GPU ID，类别数量"
    },
    "detection": {
        "confidence_threshold": 0.35,
        "nms_threshold": 0.45
        "note": "置信度阈值，NMS非极大值抑制阈值"
    },
    "tracking": {
        "enable_tracking": true,
        "frame_rate": 30,
        "track_buffer": 240,
        "track_class": 0,
        "track_thresh": 0.15,
        "high_thresh": 0.20,
        "match_thresh": 0.9,
        "unconfirmed_thresh": 0.75,
        "low_match_thresh": 0.80
        "note": "追踪是否启用，帧率，追踪缓冲区大小，追踪类别，追踪阈值，高阈值，匹配阈值，未确认阈值，低匹配阈值"
    },
    "counting": {
        "enable_counting": true,
        "detection_line_y": 800,
        "min_target_area": 100,
        "show_label": true
    },
    "output": {
        "save_video": false,
        "save_counting_log": false,
        "show_performance_stats": true
    }
}
```

### 4) 构建（CMake）
```
cmake -S . -B build -A x64
cmake --build build --config Release
```

### 5) 运行
```
cd build/Release
Identify.exe   # 程序名以实际生成为准
```

启动后，默认生成四路推流（以 `stream_urls` 为准）：
- `rtsp://<local_ip1>:<port>/thermal1`
- `rtsp://<local_ip1>:<port>/visible1`
- `rtsp://<local_ip2>:<port>/thermal2`
- `rtsp://<local_ip2>:<port>/visible2`

## 使用说明
### 可见光检测与推流
- YOLO(TensorRT) + ByteTrack 对两路可见光独立处理；
- 推流路径：BGR → CUDA(YUV420P) → H.264（ultrafast+zerolatency，禁B帧）→ RTSP；
- 帧率控制：基于时间戳，去除固定 sleep 带来的抖动。

### 热成像处理
- 通过海康 SDK 实时测温回调提供门限参考；
- 对热成像帧进行温度条 ROI 分位数阈值估计 + 灰度阈值二值化；
- 输出 640×512 的近似温度矩阵（CV_32FC1），用于融合/显示/上报。

### RS422(GYK) → CAN(TCP广播)
- RS422 读取 GYK 帧，解析时间/速度/公里标/车次/机车号；
- 重编码为 48 字节 CAN 负载，并封装：
  - 报头 0xAA + 4 字节检测位（cam1可见/热，cam2可见/热）+ CAN48 + CRC16(0xA001) + 报尾 0xFF；
- 通过内置 TCP 服务器广播给前端；串口异常时自动使用上一帧或模拟数据保障连续性。

### 控制服务器（多前端一致）
- 监听端口：默认 `12347`（可在源码中调整或扩展到配置）
- 前端任意一端发送：`CMD:SET_DIR:1\n` 或 `CMD:SET_DIR:2\n`
- 后端广播给所有客户端：`NOTIFY:SHOW_DIR:1\n` 或 `NOTIFY:SHOW_DIR:2\n`

## 性能优化建议
- 编码：H.264 `ultrafast` + `zerolatency`，禁用 B 帧，`gop_size=fps`；
- 网络：RTSP 走 TCP，启用 `tcp_nodelay`，适度减小 `buffer_size`；
- 预处理：CUDA BGR→YUV420P，避免 CPU 瓶颈；
- 帧率：基于时间戳的精准帧控，替代固定 `sleep`；
- 存储：录像走独立预览句柄与线程，后台容量守护，避免影响实时链路。

## 故障排查
1. RTSP 连接成功但无数据：
   - 确认 `rtsp-simple-server.exe` 已运行且端口未被占用；
   - 检查 `stream_urls` 与实际拉流地址一致；
2. 画面延迟或卡顿：
   - 确认启用 CUDA 转换与低延迟编码参数；
   - 降低分辨率或帧率；
3. TCP 上报未收到：
   - 检查前端是否成功连接后端 TCP 服务器（端口参考配置/源码）；
   - 查看 `utils/sent_packets_log.txt`（如开启日志保存）；
4. 海康登录失败：
   - 核对 `config.json` 设备 IP、端口、账号、密码；
   - 网络可达性与防火墙；

## 许可证与第三方
- 项目代码以自定义条款/公司内部规范为准（请根据实际选择 License）。
- 海康 SDK、FFmpeg、OpenCV、CUDA 等第三方库请遵循各自授权协议。

## 贡献
欢迎提交 Issue 与 PR：
- 修复 Bug、改进性能与稳定性
- 优化配置可观测性（指标、日志、开关）
- 拓展前端协议或数据格式

## 致谢
感谢开源社区与设备厂商提供的优秀工具链与文档支持。


