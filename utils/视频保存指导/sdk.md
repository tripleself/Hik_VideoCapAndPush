### NET_DVR_SaveRealData

捕获数据并存放到指定的文件中。

```cpp
BOOL NET_DVR_SaveRealData(
  LONG    lRealHandle,
  char    *sFileName
);
```

#### Parameters

*lRealHandle*
NET_DVR_RealPlay或NET_DVR_RealPlay_V30的返回值
*sFileName*
文件路径指针，包括文件名，例如："D:\\test.mp4"
#### Return Values
TRUE表示成功，FALSE表示失败。接口返回失败请调用**NET_DVR_GetLastError**获取错误码，通过错误码判断出错原因。

#### Remarks

V5.0.3.2或以后版本，通过该接口保存录像，保存的录像文件数据超过文件最大限制字节数（默认为1024MB），SDK会自动切片，即新建文件进行保存，文件名命名规则为“在接口传入的文件名基础上增加数字标识(例如：*_1.mp4、*_2.mp4)”。可以调用NET_DVR_GetSDKLocalCfg、NET_DVR_SetSDKLocalCfg(配置类型：NET_DVR_LOCAL_CFG_TYPE_GENERAL)获取和设置切片模式和文件最大限制字节数。
#### See Also
**NET_DVR_RealPlay_V40** **NET_DVR_StopSaveRealData**

### NET_DVR_SaveRealData_V30

按指定的目标封装格式捕获数据并存放到指定的文件中。

```cpp
BOOL NET_DVR_SaveRealData_V30(
LONG   lRealHandle
DWORD dwTransType
char   *sFileName 
```

#### Parameters

*lRealHandle*
[in] NET_DVR_RealPlay_V40等接口的返回值
*dwTransType*
[in] 存储的码流封装格式，见STREAM_TYPE定义

```cpp
enum STREAM_TYPE{
  STREAM_PS     = 0x1,
  STREAM_3GPP   = 0x2
}
```

*STREAM_PS*
PS文件层，主要用于存储，也可用于传输
*STREAM_3GPP*
3GPP文件层，用于存储
*sFileName*
文件路径指针，绝对路径，包括文件名

#### Return Values

TRUE表示成功，FALSE表示失败。接口返回失败请调用**NET_DVR_GetLastError**获取错误码，通过错误码判断出错原因。

#### Remarks

* 该接口目前仅对支持RTSP协议取流的设备有效。
* V5.0.3.2或以后版本，通过该接口保存录像，保存的录像文件数据超过文件最大限制字节数（默认为1024MB），SDK会自动切片，即新建文件进行保存，文件名命名规则为“在接口传入的文件名基础上增加数字标识(例如：*_1.mp4、*_2.mp4)”。可以调用 **NET_DVR_GetSDKLocalCfg** 、 **NET_DVR_SetSDKLocalCfg** (配置类型：NET_DVR_LOCAL_CFG_TYPE_GENERAL)获取和设置切片模式和文件最大限制字节数。

#### See Also

**NET_DVR_RealPlay_V40** **NET_DVR_StopSaveRealData**

### NET_DVR_StopSaveRealData

停止数据捕获。

```cpp
BOOL NET_DVR_StopSaveRealData(
  LONG    lRealHandle
);
```

#### Parameters

*lRealHandle*
NET_DVR_RealPlay或NET_DVR_RealPlay_V30的返回值

#### Return Values

TRUE表示成功，FALSE表示失败。接口返回失败请调用**NET_DVR_GetLastError**获取错误码，通过错误码判断出错原因。

#### See Also

**NET_DVR_RealPlay** **NET_DVR_RealPlay_V30** **NET_DVR_SaveRealData**

### NET_DVR_GetSDKLocalCfg

获取SDK本地参数。

```cpp
BOOL NET_DVR_GetSDKLocalCfg(
  NET_SDK_LOCAL_CFG_TYPE    enumType,
  void                      *lpOutBuff
);
```

#### Parameters

*enumType*
[in] 配置类型，不同的取值对应不同的SDK参数，详见下表
*lpOutBuff*
[out] 输出参数，不同的配置类型，输出参数对应不同的结构

enumType
配置类型，不同的取值对应不同的SDK参数，详见下表
lpOutBuff
输出参数，不同的配置类型，输出参数对应不同的结构

| enumType | 类型值 | 含义 | lpOutBuff对应结构体 |
| -------- | ---- | ---- | ---- |
|NET_SDK_LOCAL_CFG_TYPE_TCP_PORT_BIND| 0 | 本地TCP端口绑定配置 | NET_DVR_LOCAL_TCP_PORT_BIND_CFG |
|NET_SDK_LOCAL_CFG_TYPE_UDP_PORT_BIND| 1 | 本地UDP端口绑定配置 | NET_DVR_LOCAL_UDP_PORT_BIND_CFG |
|NET_SDK_LOCAL_CFG_TYPE_MEM_POOL| 2 | 内存池本地配置 | NET_DVR_LOCAL_MEM_POOL_CFG |
|NET_SDK_LOCAL_CFG_TYPE_MODULE_RECV_TIMEOUT| 3 | 按模块配置超时时间 | NET_DVR_LOCAL_MODULE_RECV_TIMEOUT_CFG |
|NET_SDK_LOCAL_CFG_TYPE_ABILITY_PARSE| 4 | 是否使用能力集解析库 | NET_DVR_LOCAL_ABILITY_PARSE_CFG |
|NET_SDK_LOCAL_CFG_TYPE_TALK_MODE| 5 | 对讲模式配置 | NET_DVR_LOCAL_TALK_MODE_CFG |
|NET_SDK_LOCAL_CFG_TYPE_CHECK_DEV| 10 | 心跳交互间隔时间配置 | NET_DVR_LOCAL_CHECK_DEV |
|NET_DVR_LOCAL_CFG_TYPE_GENERAL| 17 | 通用参数配置 | NET_DVR_LOCAL_GENERAL_CFG |
|NET_DVR_LOCAL_CFG_TYPE_PTZ| 18 | PTZ是否接收设备返回配置 | NET_DVR_LOCAL_CFG_TYPE_PTZ |
|NET_SDK_LOCAL_CFG_CERTIFICATION| 20 | 证书相关参数配置 | NET_DVR_LOCAL_CERTIFICATION |
|NET_SDK_LOCAL_CFG_PORT_MULTIPLEX| 21 | 端口复用参数配置 | NET_DVR_LOCAL_PORT_MULTI_CFG |


#### Return Values

TRUE表示成功，FALSE表示失败。接口返回失败请调用**NET_DVR_GetLastError**获取错误码，通过错误码判断出错原因。

#### See Also

**NET_DVR_SetSDKLocalCfg**

### NET_DVR_SetSDKLocalCfg
设置SDK本地参数。

```cpp
BOOL NET_DVR_SetSDKLocalCfg(
  NET_SDK_LOCAL_CFG_TYPE    enumType,
  void* const               lpInBuff
);
```
#### Parameters

*enumType*
配置类型，不同的取值对应不同的SDK参数，详见下表
*lpInBuff*
输入参数，不同的配置类型，输入参数对应不同的结构

| enumType | 类型值 | 含义 | lpInBuff对应结构体 |
| -------- | ---- | ---- | ---- |
|NET_SDK_LOCAL_CFG_TYPE_TCP_PORT_BIND| 0 | 本地TCP端口绑定配置 | NET_DVR_LOCAL_TCP_PORT_BIND_CFG |
|NET_SDK_LOCAL_CFG_TYPE_UDP_PORT_BIND| 1 | 本地UDP端口绑定配置 | NET_DVR_LOCAL_UDP_PORT_BIND_CFG |
|NET_SDK_LOCAL_CFG_TYPE_MEM_POOL| 2 | 内存池本地配置 | NET_DVR_LOCAL_MEM_POOL_CFG |
|NET_SDK_LOCAL_CFG_TYPE_MODULE_RECV_TIMEOUT| 3 | 按模块配置超时时间 | NET_DVR_LOCAL_MODULE_RECV_TIMEOUT_CFG |
|NET_SDK_LOCAL_CFG_TYPE_ABILITY_PARSE| 4 | 是否使用能力集解析库 | NET_DVR_LOCAL_ABILITY_PARSE_CFG |
|NET_SDK_LOCAL_CFG_TYPE_TALK_MODE| 5 | 对讲模式配置 | NET_DVR_LOCAL_TALK_MODE_CFG |
|NET_SDK_LOCAL_CFG_TYPE_CHECK_DEV| 10 | 心跳交互间隔时间配置 | NET_DVR_LOCAL_CHECK_DEV |
|NET_SDK_LOCAL_CFG_TYPE_CHAR_ENCODE| 13 | 配置字符编码相关处理回调 | NET_DVR_LOCAL_BYTE_ENCODE_CONVERT |
|NET_DVR_LOCAL_CFG_TYPE_LOG| 15 | 日志参数配置 | NET_DVR_LOCAL_LOG_CFG |
|NET_DVR_LOCAL_CFG_TYPE_GENERAL| 17 | 通用参数配置 | NET_DVR_LOCAL_GENERAL_CFG |
|NET_DVR_LOCAL_CFG_TYPE_PTZ| 18 | PTZ是否接收设备返回配置 | NET_DVR_LOCAL_CFG_TYPE_PTZ |
|NET_SDK_LOCAL_CFG_CERTIFICATION| 20 | 证书相关参数配置 | NET_DVR_LOCAL_CERTIFICATION |
|NET_SDK_LOCAL_CFG_PORT_MULTIPLEX| 21 | 端口复用参数配置 | NET_DVR_LOCAL_PORT_MULTI_CFG |

#### Return Values

TRUE表示成功，FALSE表示失败。接口返回失败请调用NET_DVR_GetLastError获取错误码，通过错误码判断出错原因。

####See Also

**NET_DVR_GetSDKLocalCfg**

### NET_DVR_LOCAL_GENERAL_CFG
通用参数配置结构体。

```cpp
struct{
  BYTE      byExceptionCbDirectly; 
  BYTE      byNotSplitRecordFile;
  BYTE      byResumeUpgradeEnable; 
  BYTE      byAlarmJsonPictureSeparate;
  BYTE      byRes[4];
  UINT64    i64FileSize;
  UINT32    dwResumeUpgradeTimeout;
  BYTE      byRes1[236]; 
}NET_DVR_LOCAL_GENERAL_CFG, *LPNET_DVR_LOCAL_GENERAL_CFG;
```

#### Members

`byExceptionCbDirectly`
异常回调类型：0- 通过线程池异常回调，1- 直接异常回调给上层 
`byNotSplitRecordFile`
回放和预览中保存到本地录像文件不切片：0- 切片（默认），1- 不切片 
`byResumeUpgradeEnable`
断网续传升级使能：0-关闭（默认），1-开启 
`byAlarmJsonPictureSeparate`
控制JSON透传报警数据和图片是否分离，0-不分离，1-分离（分离后走COMM_ISAPI_ALARM回调返回） 
`byRes`
保留 
`i64FileSize`
文件最大限制字节数，单位：Byte，启用切片（byNotSplitRecordFile为0）时，预览和回放保存的录像文件超过这个大小限制会自动切片，即新建文件进行保存 
`dwResumeUpgradeTimeout`
断网续传重连超时时间，单位毫秒 
`byRes1`
保留 

#### See Also

**NET_DVR_GetSDKLocalCfg** **NET_DVR_SetSDKLocalCfg**



