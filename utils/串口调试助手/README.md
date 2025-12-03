# GYK公用数据箱输出通讯协议解析器

这是一个基于C++的RS422串口通信协议数据解析程序，专门用于解析GYK公用数据箱输出的通讯数据。程序采用模块化设计，便于集成到其他项目中。

## 功能特性

- **严格协议解析**: 根据序号重排.md文档严格解析RS422串口通信协议数据帧
- **精确数据提取**: 提取关键信息包括：
  - 时间（年月日时分秒）- 序号46，位置45-48
  - 实速 - 序号50，位置49-51  
  - 公里标 - 序号58，位置57-59
  - 五位车次 - 序号67，位置66-67
  - 机车号 - 序号75，位置74-75
- **模块化设计**: 代码结构清晰，便于移植和集成
- **测试支持**: 支持从txt文件读取数据进行测试（无需真实RS422硬件）
- **TCP通信**: 提供TCP服务器和客户端用于数据传输
- **Windows优化**: 专门针对Windows平台优化

## 项目结构

```
GYKProtocolParser/
├── CMakeLists.txt              # CMake构建配置文件
├── README.md                   # 项目说明文档
├── 序号重排.md                 # 协议文档（字节偏移位置）
├── 时间解析.md                 # 时间字段解析说明
├── include/
│   ├── ProtocolParser.h        # 协议解析器头文件
│   ├── GYKProtocolConfig.h     # 协议配置常量
│   └── RS422Interface.h        # RS422串口接口（可选）
├── src/
│   ├── ProtocolParser.cpp      # 协议解析器实现
│   ├── tcp_server.cpp          # TCP服务器实现
│   └── tcp_client.cpp          # TCP客户端实现
├── SaveWindows*.TXT            # 真实数据样本（用于测试）
└── build/                      # 编译输出目录
```

## 编译和构建

### 前置要求

- CMake 3.10 或更高版本
- C++17 兼容的编译器 (GCC, Clang, MSVC)
- Windows系统需要Winsock2库支持

### 构建步骤

1. 克隆或下载项目到本地
2. 创建构建目录：
   ```bash
   mkdir build
   cd build
   ```
3. 生成构建文件：
   ```bash
   cmake ..
   ```
4. 编译项目：
   ```bash
   cmake --build .
   ```

### Windows构建示例

```cmd
mkdir build
cd build
cmake ..
cmake --build . --config Release
```

### Linux构建示例

```bash
mkdir build
cd build
cmake ..
make
```

## 使用方法

### 1. 启动TCP服务器

```bash
# 在build/bin目录下
./tcp_server      # Linux
tcp_server.exe    # Windows
```

服务器默认监听端口8888，启动后会：
- 模拟从串口读取协议数据
- 解析数据帧并提取关键信息
- 将解析结果以JSON格式发送给连接的客户端
- 每2秒发送一次数据

### 2. 启动TCP客户端

```bash
# 在build/bin目录下
./tcp_client      # Linux
tcp_client.exe    # Windows
```

客户端启动后会：
- 提示输入服务器IP地址（默认127.0.0.1）
- 提示输入端口号（默认8888）
- 连接到服务器并接收数据
- 格式化显示接收到的数据

### 3. 数据格式

解析后的数据以JSON格式传输：

```json
{
  "trainNumber": "123456",
  "dateTime": "2024-01-15 14:25:30",
  "actualSpeed": 120.5,
  "kilometerPost": 1234.500,
  "locomotiveNumber": "136",
  "carNumberSupplement": "153",
  "isValid": true
}
```

## RS422串口协议详解

### 通信参数
- **接口类型**: RS422差分串口
- **波特率**: 9600bps
- **数据位**: 8位
- **停止位**: 1位
- **奇偶校验**: 无
- **流控**: 无

### 数据帧格式
根据序号重排.md文档，数据帧结构如下：

| 字段 | 位置 | 长度 | 说明 |
|------|------|------|------|
| 帧起始 | 0-1 | 2字节 | DLE(10H) + STX(02H) |
| 信息长度 | 2-3 | 2字节 | 低字节在前 |
| 时间 | 45-48 | 4字节 | 年月日时分秒，BCD编码 |
| 实速 | 49-51 | 3字节 | 低字节在前，单位0.1km/h |
| 公里标 | 57-59 | 3字节 | 低字节在前，单位米 |
| 五位车次 | 66-67 | 2字节 | 低字节在前 |
| 机车号 | 74-75 | 2字节 | 低字节在前 |
| CRC校验 | 83-84 | 2字节 | CRC16校验 |
| 帧结束 | 85-86 | 2字节 | DLE(10H) + ETX(03H) |

### 时间字段解析
时间字段采用特殊的位域编码方式：
- b5-b0: 秒 (6位)
- b11-b6: 分 (6位) 
- b16-b12: 时 (5位)
- b21-b17: 日 (5位)
- b25-b22: 月 (4位)
- b31-b26: 年 (6位，需加2000)

详细解析示例请参考`时间解析.md`文档。

## 主要类和接口

### ProtocolParser类

协议解析器的核心类，提供以下主要接口：

```cpp
// 解析数据帧
ParsedData parseFrame(const uint8_t* data, size_t length);

// 转换为JSON字符串
std::string toJsonString(const ParsedData& data);
```

### ParsedData结构

存储解析后的数据：

```cpp
struct ParsedData {
    std::string trainNumber;        // 车次数字部分
    std::string dateTime;           // 年月日时分秒
    double actualSpeed;             // 实速 (km/h)
    double kilometerPost;           // 公里标
    std::string locomotiveNumber;   // 机车号
    std::string carNumberSupplement;// 车号补充
    bool isValid;                   // 数据是否有效
};
```

## 项目移植指导

### 集成到现有项目

#### 1. 核心文件集成
将以下文件复制到目标项目：
```
include/ProtocolParser.h        # 必需
include/GYKProtocolConfig.h     # 必需
src/ProtocolParser.cpp          # 必需
```

#### 2. 基本使用示例
```cpp
#include "ProtocolParser.h"

// 创建解析器实例
ProtocolParser parser;

// 解析数据帧
uint8_t rawData[87] = { /* 从RS422接收的数据 */ };
auto result = parser.parseFrame(rawData, sizeof(rawData));

if (result.isValid) {
    std::cout << "时间: " << result.dateTime << std::endl;
    std::cout << "速度: " << result.actualSpeed << " km/h" << std::endl;
    std::cout << "公里标: " << result.kilometerPost << " km" << std::endl;
    std::cout << "车次: " << result.fiveDigitTrainNumber << std::endl;
    std::cout << "机车号: " << result.locomotiveNumber << std::endl;
}
```

#### 3. Windows RS422串口集成

对于真实的RS422串口通信，需要使用Windows API：

```cpp
#include <windows.h>

class RS422Reader {
private:
    HANDLE hSerial;
    
public:
    bool openPort(const std::string& portName) {
        hSerial = CreateFileA(portName.c_str(),
                            GENERIC_READ | GENERIC_WRITE,
                            0, 0, OPEN_EXISTING,
                            FILE_ATTRIBUTE_NORMAL, 0);
        
        if (hSerial == INVALID_HANDLE_VALUE) return false;
        
        DCB dcbSerialParams = {0};
        dcbSerialParams.DCBlength = sizeof(dcbSerialParams);
        
        if (!GetCommState(hSerial, &dcbSerialParams)) return false;
        
        dcbSerialParams.BaudRate = CBR_9600;
        dcbSerialParams.ByteSize = 8;
        dcbSerialParams.StopBits = ONESTOPBIT;
        dcbSerialParams.Parity = NOPARITY;
        
        return SetCommState(hSerial, &dcbSerialParams);
    }
    
    int readData(uint8_t* buffer, int maxLength) {
        DWORD bytesRead;
        if (ReadFile(hSerial, buffer, maxLength, &bytesRead, NULL)) {
            return bytesRead;
        }
        return -1;
    }
};
```

### 移植注意事项

#### 1. 字节序处理
协议使用小端字节序（低字节在前），在不同平台上需要注意：
```cpp
// 正确的小端字节序读取
uint16_t value = data[0] | (data[1] << 8);  // 低字节在前
```

#### 2. 配置常量
所有协议相关的常量都定义在`GYKProtocolConfig.h`中，便于维护：
```cpp
// 使用配置常量而不是硬编码
if (length > GYKProtocol::POS_DATE_TIME + GYKProtocol::LEN_DATE_TIME - 1) {
    result.dateTime = parseBCDTime(&data[GYKProtocol::POS_DATE_TIME]);
}
```

#### 3. 错误处理
建议在生产环境中增强错误处理：
```cpp
try {
    auto result = parser.parseFrame(data, length);
    if (!result.isValid) {
        // 处理解析失败
        logError("Protocol parsing failed");
    }
} catch (const std::exception& e) {
    // 处理异常
    logError("Exception: " + std::string(e.what()));
}
```

#### 4. 线程安全
如果在多线程环境中使用，需要添加同步机制：
```cpp
#include <mutex>

class ThreadSafeParser {
private:
    ProtocolParser parser;
    std::mutex mtx;
    
public:
    ProtocolParser::ParsedData parseFrame(const uint8_t* data, size_t length) {
        std::lock_guard<std::mutex> lock(mtx);
        return parser.parseFrame(data, length);
    }
};
```

### Windows平台特定说明

#### 1. RS422硬件要求
- 支持RS422差分信号的串口卡或USB转RS422适配器
- 正确连接A+、B-、地线
- 确认设备管理器中串口驱动正常

#### 2. 编译环境
- Visual Studio 2019或更高版本
- CMake 3.10或更高版本
- Windows SDK

#### 3. 调试建议
- 使用串口调试工具验证数据接收
- 启用详细日志输出
- 使用提供的txt文件进行离线测试

### 测试和验证

#### 1. 离线测试
使用提供的SaveWindows*.TXT文件进行测试：
```cpp
// 从文件读取测试数据
std::ifstream file("SaveWindows2025_8_18_16-11-01.TXT");
// 解析十六进制数据并测试
```

#### 2. 在线测试
连接真实RS422设备进行测试：
```cpp
RS422Reader reader;
if (reader.openPort("COM1")) {
    uint8_t buffer[1024];
    int bytesRead = reader.readData(buffer, sizeof(buffer));
    if (bytesRead > 0) {
        auto result = parser.parseFrame(buffer, bytesRead);
        // 验证解析结果
    }
}
```

## 许可证

本项目采用MIT许可证，详见LICENSE文件。

## 联系方式

如有问题或建议，请通过以下方式联系：
- 提交Issue到项目仓库
- 发送邮件到开发者邮箱

---

**注意**: 本程序仅用于学习和演示目的，实际应用时请根据具体的协议规范和安全要求进行调整。

