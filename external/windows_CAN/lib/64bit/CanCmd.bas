Attribute VB_Name = "CanCmd"
'定义需要用到的数据结构

'//CAN数据帧类型
Public Type PCAN_DataFrame
    uTimeFlag       As Long                     ' // 时间标识,对接收帧有效
    nSendType       As Byte                     ' // 发送帧类型,0-正常发送;1-单次发送;2-自发自收;3-单次自发自收
    bRemoteFlag     As Byte                     ' // 是否是远程帧
    bExternFlag     As Byte                     ' // 是否是扩展帧
    nDataLen        As Byte                     ' // 数据长度
    uID             As Long                     ' // 报文DI
    arryData(7)     As Byte                     ' // 报文数据
End Type

'//CAN初始化配置
Public Type PCAN_InitConfig
    bMode           As Byte                     '// 工作模式(0表示正常模式,1表示只听模式)
    nBtrType        As Byte '                   // 位定时参数模式(1表示SJA1000,0表示LPC21XX)
    dwBtr(3)        As Byte '                   // CAN位定时参数
    dwAccCode       As Long '                   // 验收码
    dwAccMask       As Long '                   // 屏蔽码
    nFilter         As Byte '                   // 滤波方式(0表示未设置滤波功能,1表示双滤波,2表示单滤波
    dwReserved      As Byte '                   // 预留字段
End Type

'//CAN设备信息
Public Type PCAN_DeviceInformation
    uHardWareVersion    As Integer '            // 硬件版本
    uFirmWareVersion    As Integer '            // 固件版本
    uDriverVersion      As Integer '            // 驱动版本
    uInterfaceVersion   As Integer '            // 接口库版本
    uInterruptNumber    As Integer '            // 中断号
    bChannelNumber      As Byte '               // 有几路CAN
    szSerialNumber(19)  As Byte '               // 设备序列号
    szHardWareType(39)  As Byte '               // 硬件类型
    szDescription(19)   As Byte '               // 设备描述
End Type


'//CAN错误信息
Public Type PCAN_ErrorInformation
    uErrorCode      As Long '                   // 错误类型
    PassiveErrData(3)   As Byte '               // 消极错误数据
    ArLostErrData   As Byte '                   // 仲裁错误数据
End Type

'// 打开设备
Declare Function CAN_DeviceOpen Lib "CanCmd.dll" (ByVal dwType As Long, ByVal dwIndex As Long, ByVal pDescription As Long) As Long

'// 关闭设备
Declare Function CAN_DeviceClose Lib "CanCmd.dll" (ByVal dwDeviceHandle As Long) As Long

'// 启动CAN
Declare Function CAN_ChannelStart Lib "CanCmd.dll" (ByVal dwDeviceHandle As Long, ByVal dwChannel As Long, ByRef pInitConfig As PCAN_InitConfig) As Long

'// 停止CAN
Declare Function CAN_ChannelStop Lib "CanCmd.dll" (ByVal dwDeviceHandle As Long, ByVal dwChannel As Long) As Long

'// 获取设备信息
Declare Function CAN_GetDeviceInfo Lib "CanCmd.dll" (ByVal dwDeviceHandle As Long, ByRef pInfo As PCAN_DeviceInformation) As Long

'// 获取CAN错误信息
Declare Function CAN_GetErrorInfo Lib "CanCmd.dll" (ByVal dwDeviceHandle As Long, ByVal dwChannel As Long, ByRef pErr As PCAN_ErrorInformation) As Long

'// 发送数据
Declare Function CAN_ChannelSend Lib "CanCmd.dll" (ByVal dwDeviceHandle As Long, ByVal dwChannel As Long, ByRef pSend As PCAN_DataFrame, ByVal nCount As Long) As Long

'// 从接收缓冲区中读数据
' nWaitTime 在调用前赋值为 -1，
Declare Function CAN_ChannelReceive Lib "CanCmd.dll" (ByVal dwDeviceHandle As Long, ByVal dwChannel As Long, ByRef pReceive As PCAN_DataFrame, ByVal nCount As Long, ByVal nWaitTime As Integer) As Long

'// 获取接收缓冲区帧数
Declare Function CAN_GetReceiveCount Lib "CanCmd.dll" (ByVal dwDeviceHandle As Long, ByVal dwChannel As Long) As Long

'// 清空接收缓冲区
Declare Function CAN_ClearReceiveBuffer Lib "CanCmd.dll" (ByVal dwDeviceHandle As Long, ByVal dwChannel As Long) As Long

'// 读寄存器
Declare Function CAN_ReadRegister Lib "CanCmd.dll" (ByVal dwDeviceHandle As Long, ByVal dwChannel As Long, ByVal dwAddr As Long, ByRef pBuff As Byte, ByVal nLen As Integer) As Long

'// 写寄存器
Declare Function CAN_WriteRegister Lib "CanCmd.dll" (ByVal dwDeviceHandle As Long, ByVal dwChannel As Long, ByVal dwAddr As Long, ByRef pBuff As Byte, ByVal nLen As Integer) As Long

'// 获取参数
Declare Function CAN_GetParam Lib "CanCmd.dll" (ByVal dwDeviceHandle As Long, ByVal dwChannel As Long, ByVal dwParamType As Long, ByRef pData As Byte) As Long

'// 设置参数
Declare Function CAN_SetParam Lib "CanCmd.dll" (ByVal dwDeviceHandle As Long, ByVal dwChannel As Long, ByVal dwParamType As Long, ByRef pData As Byte) As Long
