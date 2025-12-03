# 查看OpenCV编码器支持指南

## 🎯 目标
帮助您查看OpenCV 4.11.0版本支持哪些FFmpeg编码器，并选择最适合的编码器。

## 🔍 方法1：运行程序自动检测（推荐）

### 已集成的自动检测功能
我已经在您的 `VideoSaveManager` 中添加了自动编码器检测功能。

**运行程序时会自动显示：**
```bash
cd build/Release
./Identify.exe
```

**预期输出：**
```
[VideoSaveManager] === OpenCV Build Info ===
OpenCV Version: 4.11.0

[VideoSaveManager] === Testing Video Codecs ===
✅ MJPEG - SUPPORTED (fourcc: 1196444237)
❌ H264 - NOT SUPPORTED (fourcc: 875967048)
❌ X264 - NOT SUPPORTED (fourcc: 875967064)
✅ XVID - SUPPORTED (fourcc: 1145656920)
✅ MP4V - SUPPORTED (fourcc: 1446269005)
❌ WMV2 - NOT SUPPORTED (fourcc: 844313175)
❌ DIVX - NOT SUPPORTED (fourcc: 1482049860)
✅ Uncompressed - SUPPORTED (fourcc: 0)

[VideoSaveManager] === OpenCV Build Information ===
  Video I/O:
    FFMPEG:                      YES
      avcodec:                   YES (58.134.100)
      avformat:                  YES (58.76.100)
      avutil:                    YES (56.70.100)
      swscale:                   YES (5.9.100)
      avresample:                NO
[VideoSaveManager] === Codec Test Complete ===
```

## 🔍 方法2：独立测试程序

### 编译独立测试程序
我已经为您创建了独立的测试程序：

```bash
# 在utils目录下
cd utils
mkdir codec_test_build
cd codec_test_build

# 复制CMakeLists文件
cp ../CMakeLists_codec_test.txt ./CMakeLists.txt

# 复制测试程序
cp ../check_opencv_codecs.cpp ./

# 编译
cmake ..
cmake --build . --config Release

# 运行
./codec_test.exe
```

## 🔍 方法3：通过FFmpeg命令行查看

### 查看FFmpeg编码器列表
```bash
# 查看所有编码器
ffmpeg -encoders

# 查看视频编码器
ffmpeg -encoders | grep Video

# 查看特定编码器
ffmpeg -encoders | grep h264
ffmpeg -encoders | grep mjpeg
```

### 常见编码器检查
```bash
# 检查H.264支持
ffmpeg -encoders | findstr "264"

# 检查MJPEG支持  
ffmpeg -encoders | findstr "mjpeg"

# 检查XVID支持
ffmpeg -encoders | findstr "xvid"
```

## 📊 编码器选择建议

### 基于您的OpenCV 4.11.0版本

#### 🥇 推荐选择（按优先级）

1. **MJPEG** (当前使用)
   ```cpp
   cv::VideoWriter::fourcc('M', 'J', 'P', 'G')
   ```
   - ✅ 兼容性最好
   - ✅ 实时性能好
   - ⚠️ 文件较大

2. **XVID** (如果支持)
   ```cpp
   cv::VideoWriter::fourcc('X', 'V', 'I', 'D')
   ```
   - ✅ 较好的压缩率
   - ✅ 广泛支持
   - ✅ 文件大小适中

3. **MP4V** (如果支持)
   ```cpp
   cv::VideoWriter::fourcc('M', 'P', '4', 'V')
   ```
   - ✅ 标准MPEG-4编码
   - ✅ 较好的兼容性

#### 🔄 动态编码器选择

让我为您创建一个智能编码器选择函数：

```cpp
int VideoSaveManager::selectBestCodec()
{
    // 按优先级测试编码器
    std::vector<std::pair<std::string, int>> codecs = {
        {"H264", cv::VideoWriter::fourcc('H', '2', '6', '4')},  // 最佳压缩
        {"XVID", cv::VideoWriter::fourcc('X', 'V', 'I', 'D')},  // 较好压缩
        {"MP4V", cv::VideoWriter::fourcc('M', 'P', '4', 'V')},  // 标准MPEG-4
        {"MJPEG", cv::VideoWriter::fourcc('M', 'J', 'P', 'G')}, // 最佳兼容性
    };
    
    for (const auto& codec : codecs)
    {
        cv::VideoWriter test("test.avi", codec.second, 25.0, cv::Size(640, 480), true);
        if (test.isOpened())
        {
            test.release();
            std::remove("test.avi");
            std::cout << "Selected codec: " << codec.first << std::endl;
            return codec.second;
        }
    }
    
    // 回退到无压缩
    return 0;
}
```

## 🛠️ 根据检测结果优化配置

### 如果MJPEG可用（当前状态）
```json
{
  "video_save": {
    "max_file_size_gb": 3,      // 增加文件大小限制
    "max_storage_gb": 500,      // 增加总存储
    "cleanup_size_gb": 100      // 增加清理大小
  }
}
```

### 如果H.264可用（理想状态）
```json
{
  "video_save": {
    "max_file_size_gb": 1,      // 保持原设置
    "max_storage_gb": 200,      // 保持原设置
    "cleanup_size_gb": 40       // 保持原设置
  }
}
```

### 如果XVID可用（平衡选择）
```json
{
  "video_save": {
    "max_file_size_gb": 2,      // 适中设置
    "max_storage_gb": 300,      // 适中设置
    "cleanup_size_gb": 60       // 适中设置
  }
}
```

## 🧪 测试步骤

### 1. 运行自动检测
```bash
cd build/Release
./Identify.exe
```

### 2. 观察输出
查看哪些编码器显示 `✅ SUPPORTED`

### 3. 选择最佳编码器
根据支持情况，按以下优先级选择：
1. H264 > XVID > MP4V > MJPEG > Uncompressed

### 4. 更新代码（如需要）
如果发现更好的编码器，修改：
```cpp
// 在 VideoSaveManager.cpp 中
const int VideoSaveManager::DEFAULT_FOURCC = cv::VideoWriter::fourcc('X', 'V', 'I', 'D'); // 例如改为XVID
```

## 📋 常见问题排查

### Q: 所有编码器都显示 NOT SUPPORTED
**A:** 检查FFmpeg库是否正确安装和链接

### Q: H.264显示不支持但想使用
**A:** 需要重新编译OpenCV with FFmpeg，或使用预编译的完整版本

### Q: 文件太大怎么办
**A:** 
1. 降低分辨率 (1280x720 → 640x480)
2. 降低帧率 (25fps → 15fps)  
3. 选择压缩率更高的编码器

### Q: 如何验证编码器真正工作
**A:** 
1. 检查生成的视频文件大小
2. 用VLC播放器测试播放
3. 观察CPU使用率

---

**当前状态**: ✅ 自动检测已集成  
**下一步**: 运行程序查看检测结果  
**优化建议**: 根据检测结果选择最佳编码器
