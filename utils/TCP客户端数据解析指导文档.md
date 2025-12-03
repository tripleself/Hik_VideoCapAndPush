# TCP客户端数据解析指导文档

## 概述

本文档指导前端程序如何正确连接到后端TCP服务器（端口12346）并解析接收到的车辆运行数据。后端已从CAN卡通信升级为RS422串口通信，但TCP数据包格式保持完全兼容。

## 连接信息

- **服务器地址**: `127.0.0.1` （本地）或实际部署服务器IP
- **端口号**: `12346`
- **协议**: TCP
- **数据传输**: 二进制数据包

## 数据包格式

每个数据包的完整结构如下：

```
总长度: 56字节
┌─────────────┬──────────────┬─────────────┬─────────────┬─────────────┐
│   报头      │  检测标志位   │   车辆数据   │   CRC校验   │    报尾     │
│   1字节     │    4字节     │   48字节    │   2字节     │   1字节     │
│    0xAA     │              │             │             │    0xFF     │
└─────────────┴──────────────┴─────────────┴─────────────┴─────────────┘
```

### 字段详细说明

#### 1. 报头（1字节）
- **位置**: 字节0
- **值**: `0xAA`
- **说明**: 数据包起始标识

#### 2. 检测标志位（4字节）
- **位置**: 字节1-4
- **格式**: 
  - 字节1: `camera1_visible` (0=无目标, 1=检测到目标)
  - 字节2: `camera1_thermal` (0=无目标, 1=检测到目标)  
  - 字节3: `camera2_visible` (0=无目标, 1=检测到目标)
  - 字节4: `camera2_thermal` (0=无目标, 1=检测到目标)

#### 3. 车辆运行数据（48字节）
- **位置**: 字节5-52
- **来源**: 从RS422串口的GYK协议解析并转换为CAN格式
- **详细结构**:

| 字节位置 | 长度 | 字段名称 | 数据类型 | 说明 |
|---------|------|----------|----------|------|
| 5-10 | 6字节 | 时间信息 | BCD码 | 年(后2位)-月-日-时-分-秒 |
| 11-14 | 4字节 | 车次号 | 32位整数 | 五位车次号，小端字节序 |
| 15-18 | 4字节 | 车号信息 | 字符串 | 长度+车号字符 |
| 19-22 | 4字节 | 公里标 | 32位整数 | 单位：米，包含符号和趋势位 |
| 23-24 | 2字节 | 速度 | 16位整数 | 单位：0.1km/h，小端字节序 |
| 25-52 | 28字节 | 其他信息 | 混合 | 车站号、线路号、限速、司机号等 |

#### 4. CRC校验（2字节）
- **位置**: 字节53-54
- **算法**: CRC16，多项式0xA001
- **范围**: 覆盖报头到车辆数据部分（字节0-52）
- **字节序**: 小端（低字节在前）

#### 5. 报尾（1字节）
- **位置**: 字节55
- **值**: `0xFF`
- **说明**: 数据包结束标识

## 关键数据解析

### 时间信息解析（字节5-10）
```python
def parse_time(data):
    year = 2000 + data[0]  # 年份后两位 + 2000
    month = data[1]        # 月份 1-12
    day = data[2]          # 日期 1-31
    hour = data[3]         # 小时 0-23
    minute = data[4]       # 分钟 0-59
    second = data[5]       # 秒钟 0-59
    return f"{year:04d}-{month:02d}-{day:02d} {hour:02d}:{minute:02d}:{second:02d}"
```

### 车次号解析（字节11-14）
```python
def parse_train_number(data):
    # 小端字节序，32位整数
    train_number = struct.unpack('<I', data[0:4])[0]
    return train_number
```

### 公里标解析（字节19-22）
```python
def parse_kilometer_post(data):
    # 小端字节序，32位整数
    km_data = struct.unpack('<I', data[0:4])[0]
    
    # 提取各个位段
    absolute_value = km_data & 0x7FFFFF    # b22-b0: 绝对值（米）
    is_increasing = (km_data >> 23) & 0x1  # b23: 趋势位（1=递增）
    
    # 转换为公里
    km_post = absolute_value / 1000.0
    return km_post, is_increasing
```

### 速度解析（字节23-24）
```python
def parse_speed(data):
    # 小端字节序，16位整数，单位0.1km/h
    speed_raw = struct.unpack('<H', data[0:2])[0]
    speed_kmh = speed_raw / 10.0  # 转换为km/h
    return speed_kmh
```

## 完整解析示例

### Python示例代码

```python
import socket
import struct
import binascii

class VehicleDataParser:
    def __init__(self):
        self.socket = None
        
    def connect(self, host='127.0.0.1', port=12346):
        """连接到TCP服务器"""
        try:
            self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.socket.connect((host, port))
            print(f"已连接到 {host}:{port}")
            return True
        except Exception as e:
            print(f"连接失败: {e}")
            return False
    
    def calculate_crc16(self, data):
        """计算CRC16校验"""
        crc = 0xFFFF
        for byte in data:
            crc ^= byte
            for _ in range(8):
                if crc & 0x0001:
                    crc = (crc >> 1) ^ 0xA001
                else:
                    crc >>= 1
        return crc
    
    def parse_packet(self, packet_data):
        """解析数据包"""
        if len(packet_data) != 56:
            print(f"数据包长度错误: {len(packet_data)}, 期望56字节")
            return None
            
        # 验证报头和报尾
        if packet_data[0] != 0xAA or packet_data[55] != 0xFF:
            print("数据包格式错误：报头或报尾不正确")
            return None
        
        # 验证CRC校验
        expected_crc = self.calculate_crc16(packet_data[0:53])
        received_crc = struct.unpack('<H', packet_data[53:55])[0]
        if expected_crc != received_crc:
            print(f"CRC校验失败: 期望{expected_crc:04X}, 收到{received_crc:04X}")
            return None
        
        # 解析检测标志位
        detection_flags = {
            'camera1_visible': packet_data[1],
            'camera1_thermal': packet_data[2], 
            'camera2_visible': packet_data[3],
            'camera2_thermal': packet_data[4]
        }
        
        # 解析车辆数据
        vehicle_data = packet_data[5:53]  # 48字节车辆数据
        
        # 解析时间
        time_str = self.parse_time(vehicle_data[0:6])
        
        # 解析车次号
        train_number = struct.unpack('<I', vehicle_data[6:10])[0]
        
        # 解析公里标
        km_data = struct.unpack('<I', vehicle_data[14:18])[0]
        km_post = (km_data & 0x7FFFFF) / 1000.0
        is_increasing = bool((km_data >> 23) & 0x1)
        
        # 解析速度
        speed_raw = struct.unpack('<H', vehicle_data[18:20])[0]
        speed_kmh = speed_raw / 10.0
        
        return {
            'detection_flags': detection_flags,
            'timestamp': time_str,
            'train_number': train_number,
            'kilometer_post': km_post,
            'is_increasing': is_increasing,
            'speed_kmh': speed_kmh,
            'raw_data': binascii.hexlify(packet_data).decode('ascii')
        }
    
    def parse_time(self, time_data):
        """解析时间数据"""
        year = 2000 + time_data[0]
        month = time_data[1]
        day = time_data[2]
        hour = time_data[3]
        minute = time_data[4]
        second = time_data[5]
        return f"{year:04d}-{month:02d}-{day:02d} {hour:02d}:{minute:02d}:{second:02d}"
    
    def receive_and_parse(self):
        """接收并解析数据"""
        try:
            # 接收56字节数据包
            packet_data = self.socket.recv(56)
            if len(packet_data) == 56:
                parsed_data = self.parse_packet(packet_data)
                if parsed_data:
                    self.print_parsed_data(parsed_data)
                return parsed_data
            else:
                print(f"接收数据长度不足: {len(packet_data)}")
                return None
        except Exception as e:
            print(f"接收数据失败: {e}")
            return None
    
    def print_parsed_data(self, data):
        """打印解析结果"""
        print("=" * 50)
        print("车辆运行数据解析结果:")
        print(f"时间: {data['timestamp']}")
        print(f"车次号: {data['train_number']}")
        print(f"公里标: {data['kilometer_post']:.3f} km ({'递增' if data['is_increasing'] else '递减'})")
        print(f"速度: {data['speed_kmh']:.1f} km/h")
        print("检测标志位:")
        flags = data['detection_flags']
        print(f"  相机1可见光: {'检测到' if flags['camera1_visible'] else '无目标'}")
        print(f"  相机1热成像: {'检测到' if flags['camera1_thermal'] else '无目标'}")
        print(f"  相机2可见光: {'检测到' if flags['camera2_visible'] else '无目标'}")
        print(f"  相机2热成像: {'检测到' if flags['camera2_thermal'] else '无目标'}")
        print(f"原始数据: {data['raw_data']}")
        print("=" * 50)
    
    def close(self):
        """关闭连接"""
        if self.socket:
            self.socket.close()
            print("连接已关闭")

# 使用示例
if __name__ == "__main__":
    parser = VehicleDataParser()
    
    if parser.connect():
        try:
            while True:
                data = parser.receive_and_parse()
                if not data:
                    break
        except KeyboardInterrupt:
            print("\n用户中断")
        finally:
            parser.close()
```

### JavaScript/Node.js示例代码

```javascript
const net = require('net');

class VehicleDataParser {
    constructor() {
        this.client = null;
    }
    
    connect(host = '127.0.0.1', port = 12346) {
        return new Promise((resolve, reject) => {
            this.client = new net.Socket();
            
            this.client.connect(port, host, () => {
                console.log(`已连接到 ${host}:${port}`);
                resolve();
            });
            
            this.client.on('error', (err) => {
                console.error('连接错误:', err);
                reject(err);
            });
            
            this.client.on('data', (data) => {
                this.handleData(data);
            });
            
            this.client.on('close', () => {
                console.log('连接已关闭');
            });
        });
    }
    
    calculateCRC16(data) {
        let crc = 0xFFFF;
        for (let byte of data) {
            crc ^= byte;
            for (let i = 0; i < 8; i++) {
                if (crc & 0x0001) {
                    crc = (crc >> 1) ^ 0xA001;
                } else {
                    crc >>= 1;
                }
            }
        }
        return crc;
    }
    
    parsePacket(packetData) {
        if (packetData.length !== 56) {
            console.log(`数据包长度错误: ${packetData.length}, 期望56字节`);
            return null;
        }
        
        // 验证报头和报尾
        if (packetData[0] !== 0xAA || packetData[55] !== 0xFF) {
            console.log('数据包格式错误：报头或报尾不正确');
            return null;
        }
        
        // 验证CRC校验
        const expectedCRC = this.calculateCRC16(packetData.slice(0, 53));
        const receivedCRC = packetData.readUInt16LE(53);
        if (expectedCRC !== receivedCRC) {
            console.log(`CRC校验失败: 期望${expectedCRC.toString(16)}, 收到${receivedCRC.toString(16)}`);
            return null;
        }
        
        // 解析检测标志位
        const detectionFlags = {
            camera1_visible: packetData[1],
            camera1_thermal: packetData[2],
            camera2_visible: packetData[3],
            camera2_thermal: packetData[4]
        };
        
        // 解析车辆数据
        const vehicleData = packetData.slice(5, 53);
        
        // 解析时间
        const timestamp = this.parseTime(vehicleData.slice(0, 6));
        
        // 解析车次号
        const trainNumber = vehicleData.readUInt32LE(6);
        
        // 解析公里标
        const kmData = vehicleData.readUInt32LE(14);
        const kmPost = (kmData & 0x7FFFFF) / 1000.0;
        const isIncreasing = Boolean((kmData >> 23) & 0x1);
        
        // 解析速度
        const speedRaw = vehicleData.readUInt16LE(18);
        const speedKmh = speedRaw / 10.0;
        
        return {
            detectionFlags,
            timestamp,
            trainNumber,
            kilometerPost: kmPost,
            isIncreasing,
            speedKmh,
            rawData: packetData.toString('hex')
        };
    }
    
    parseTime(timeData) {
        const year = 2000 + timeData[0];
        const month = timeData[1];
        const day = timeData[2];
        const hour = timeData[3];
        const minute = timeData[4];
        const second = timeData[5];
        
        return `${year.toString().padStart(4, '0')}-${month.toString().padStart(2, '0')}-${day.toString().padStart(2, '0')} ${hour.toString().padStart(2, '0')}:${minute.toString().padStart(2, '0')}:${second.toString().padStart(2, '0')}`;
    }
    
    handleData(data) {
        // 处理可能的粘包情况
        let offset = 0;
        while (offset + 56 <= data.length) {
            const packet = data.slice(offset, offset + 56);
            const parsedData = this.parsePacket(packet);
            if (parsedData) {
                this.printParsedData(parsedData);
            }
            offset += 56;
        }
    }
    
    printParsedData(data) {
        console.log('='.repeat(50));
        console.log('车辆运行数据解析结果:');
        console.log(`时间: ${data.timestamp}`);
        console.log(`车次号: ${data.trainNumber}`);
        console.log(`公里标: ${data.kilometerPost.toFixed(3)} km (${data.isIncreasing ? '递增' : '递减'})`);
        console.log(`速度: ${data.speedKmh.toFixed(1)} km/h`);
        console.log('检测标志位:');
        console.log(`  相机1可见光: ${data.detectionFlags.camera1_visible ? '检测到' : '无目标'}`);
        console.log(`  相机1热成像: ${data.detectionFlags.camera1_thermal ? '检测到' : '无目标'}`);
        console.log(`  相机2可见光: ${data.detectionFlags.camera2_visible ? '检测到' : '无目标'}`);
        console.log(`  相机2热成像: ${data.detectionFlags.camera2_thermal ? '检测到' : '无目标'}`);
        console.log(`原始数据: ${data.rawData}`);
        console.log('='.repeat(50));
    }
    
    close() {
        if (this.client) {
            this.client.destroy();
        }
    }
}

// 使用示例
const parser = new VehicleDataParser();
parser.connect().then(() => {
    console.log('开始接收数据...');
}).catch(console.error);

// 优雅退出
process.on('SIGINT', () => {
    console.log('\n用户中断');
    parser.close();
    process.exit(0);
});
```

## 数据来源变化说明

### RS422串口协议特点
- **通信方式**: 异步串行通信，9600bps，8数据位，1停止位，无奇偶校验
- **协议标准**: GYK公用数据箱输出通讯协议
- **数据内容**: 包含时间、实速、公里标、五位车次、机车号等关键信息
- **帧格式**: DLE+STX开始，DLE+ETX结束，包含CRC校验

### 数据转换过程
1. **RS422数据接收**: 后端从串口读取87字节的GYK协议帧
2. **协议解析**: 解析出时间、车次、公里标、速度、机车号等字段
3. **格式转换**: 将解析数据转换为48字节CAN兼容格式
4. **TCP发送**: 组装成56字节数据包通过TCP发送

### 兼容性保证
- TCP连接方式不变（端口12346）
- 数据包总长度不变（56字节）
- 数据包结构不变（报头+标志位+48字节数据+CRC+报尾）
- 前端解析代码无需修改

## 故障排除

### 常见问题

1. **连接失败**
   - 检查服务器是否启动
   - 确认端口12346未被占用
   - 验证防火墙设置

2. **数据包长度错误**
   - 可能存在粘包或分包现象
   - 建议实现缓冲区机制处理不完整数据包

3. **CRC校验失败**
   - 检查CRC计算算法是否正确
   - 确认字节序处理（小端序）

4. **数据解析异常**
   - 验证数据包格式（报头0xAA，报尾0xFF）
   - 检查字节偏移量是否正确

### 调试建议

1. **启用详细日志**
   ```python
   import logging
   logging.basicConfig(level=logging.DEBUG)
   ```

2. **十六进制数据查看**
   ```python
   print("原始数据:", packet_data.hex())
   ```

3. **分段数据验证**
   ```python
   print("报头:", hex(packet_data[0]))
   print("检测标志:", [packet_data[i] for i in range(1, 5)])
   print("CRC:", hex(struct.unpack('<H', packet_data[53:55])[0]))
   print("报尾:", hex(packet_data[55]))
   ```

## 总结

通过本文档，前端程序可以：
1. 正确连接到TCP服务器（端口12346）
2. 解析56字节的车辆运行数据包
3. 提取关键信息：时间、车次、公里标、速度等
4. 处理检测标志位信息
5. 验证数据完整性（CRC校验）

数据来源已从CAN卡升级为RS422串口，但TCP接口保持完全兼容，前端无需修改现有代码即可正常工作。
