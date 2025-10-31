'//  CanCmd.vb
'//
'//  ~~~~~~~~~~~~
'//
'// CanCmd API
'//
'//  ~~~~~~~~~~~~
'//
'//  ------------------------------------------------------------------
'//  Author : zsj
'//	Last change: 12.06.2018 Wagner
'//
'//  Language: VB .NET
'//  ------------------------------------------------------------------
'//
'//  Copyright (C) 2009-2018  Nanjing LAIKE Electronic Technology Co. Ltd.
'//  more Info at http:'//www.njlike.com
'//
Imports System
Imports System.Text
Imports System.Runtime.InteropServices

Namespace Laike.Can.CanCmd
#Region "Enumerations"

    <Flags()> _
    Public Enum CAN_ErrorCode As UInt32
        CAN_E_NOERROR = &H0   '// 没有发现错误
        CAN_E_OVERFLOW = &H1   '// CAN控制器内部FIFO溢出
        CAN_E_ERRORALARM = &H2   '// CAN控制器错误报警
        CAN_E_PASSIVE = &H4   '// CAN控制器消极错误
        CAN_E_LOSE = &H8   '// CAN控制器仲裁丢失
        CAN_E_BUSERROR = &H10   '// CAN控制器总线错误

        CAN_E_DEVICEOPENED = &H100       '// 设备已经打开
        CAN_E_DEVICEOPEN = &H200       '// 打开设备错误
        CAN_E_DEVICENOTOPEN = &H400       '// 设备没有打开
        CAN_E_BUFFEROVERFLOW = &H800       '// 缓冲区溢出
        CAN_E_DEVICENOTEXIST = &H1000       '// 此设备不存在
        CAN_E_LOADKERNELDLL = &H2000       '// 装载动态库失败
        CAN_E_CMDFAILED = &H4000       '// 执行命令失败错误码
        CAN_E_BUFFERCREATE = &H8000       '// 内存不足
    End Enum
#End Region

#Region "Structures"
    '//1.njlikeCAN系列接口卡信息的数据类型。
    '<StructureLayout(LayoutKind.Sequential)>
    Public Structure CAN_DeviceInformation
        Public uHardWareVersion As Short
        Public uFirmWareVersion As Short
        Public uDriverVersion As Short
        Public uInterfaceVersion As Short
        Public uInterruptNumber As Short
        Public bChannelNumber As Byte
        <MarshalAs(UnmanagedType.ByValArray, SizeConst:=20)> _
        Public szSerialNumber As Byte()
        <MarshalAs(UnmanagedType.ByValArray, SizeConst:=40)> _
        Public szHardWareType As Byte()
        <MarshalAs(UnmanagedType.ByValArray, SizeConst:=20)> _
        Public szDescription As Byte()
    End Structure

'//2.定义CAN信息帧的数据类型。
    Public Structure CAN_DataFrame   '//使用不安全代码
        Public uTimeFlag As Integer
        Public nSendType As Byte
        Public bRemoteFlag As Byte '//是否是远程帧
        Public bExternFlag As Byte '//是否是扩展帧
        Public nDataLen As Byte
        Public uID As Integer

        <MarshalAs(UnmanagedType.ByValArray, SizeConst:=8)> _
        Public arryData As Byte()
    End Structure

'//4.定义错误信息的数据类型。
    Public Structure CAN_ErrorInformation
        Public uErrorCode As CAN_ErrorCode
        Public PassiveErrData1 As Byte
        Public PassiveErrData2 As Byte
        Public PassiveErrData3 As Byte
        Public ArLostErrData As Byte
    End Structure

'//5.定义初始化CAN的数据类型
    Public Structure CAN_InitConfig
        Public bMode As Byte                                       '// 工作模式(0表示正常模式1表示只听模式)'///< 工作模式(0表示正常模式1表示只听模式)
        Public nBtrType As Byte                                     '// 位定时参数模式(1表示SJA10000表示LPC21XX)'// 位定时参数模式(1表示SJA10000表示LPC21XX)
        '//<MarshalAs(UnmanagedType.ByValArray,SizeConst := 4)>       '// CAN位定时参数
        Public dwBtr0 As Byte
        Public dwBtr1 As Byte
        Public dwBtr2 As Byte
        Public dwBtr3 As Byte
        Public dwAccCode As Integer                                 '// 验收码
        Public dwAccMask As Integer                                '// 屏蔽码
        Public nFilter As Byte                                '// 滤波方式(0表示未设置滤波功能1表示双滤波2表示单滤波)
        Public dwReserved As Byte                                '// 预留字段
    End Structure
#End Region

#Region "CanCmd class"
     Public NotInheritable Class CanCmd

#Region "Parameter values definition"

        Public Const CAN_RESULT_OK As UInt32 = 1
        Public Const CAN_RESULT_ERROR As UInt32 = 0

        '// CAN 卡类型定义
        Public Const LCUSB_131B As UInt32 = 1
        Public Const LCUSB_132B As UInt32 = 2
        Public Const LCPCI_252 As UInt32 = 4
        Public Const LCMiniPcie_431 As UInt32 = 1
        Public Const LCMiniPcie_432 As UInt32 = 2
        Public Const USBCAN_1CH As UInt32 = 13
        Public Const USBCAN_C_1CH As UInt32 = 14
        Public Const USBCAN_E_1CH As UInt32 = 15
        Public Const USBCAN_E_2CH As UInt32 = 16
        Public Const MPCIeCAN_1CH As UInt32 = 17
        Public Const MPCIeCAN_2CH As UInt32 = 18
#End Region

#Region "CanCmd API Implementation"

        <DllImport("CanCmd.dll", EntryPoint:="CAN_DeviceOpen")> _
        Public Shared Function CAN_DeviceOpen(ByVal dwType As UInt32, ByVal dwIndex As UInt32, ByRef pDescription As Char) As UInt32
        End Function

        <DllImport("CanCmd.dll", EntryPoint:="CAN_DeviceClose")> _
        Public Shared Function CAN_DeviceClose(ByVal dwDeviceHandle As UInt32) As UInt32
        End Function

        <DllImport("CanCmd.dll", EntryPoint:="CAN_ChannelStart")> _
        Public Shared Function CAN_ChannelStart(ByVal dwDeviceHandle As UInt32, ByVal dwChannel As UInt32, ByRef pInitConfig As CAN_InitConfig) As UInt32
        End Function

        <DllImport("CanCmd.dll", EntryPoint:="CAN_ChannelStop")> _
        Public Shared Function CAN_ChannelStop(ByVal dwDeviceHandle As UInt32, ByVal dwChannel As UInt32) As UInt32
        End Function


        <DllImport("CanCmd.dll", EntryPoint:="CAN_GetDeviceInfo")> _
        Public Shared Function CAN_GetDeviceInfo(ByVal dwDeviceHandle As UInt32, ByRef pInfo As CAN_DeviceInformation) As UInt32
        End Function

        <DllImport("CanCmd.dll", EntryPoint:="CAN_GetParam")> _
        Public Shared Function CAN_GetParam(ByVal dwDeviceHandle As UInt32, ByVal dwChannel As UInt32, ByVal RefType As UInt32, ByRef pData As Byte) As UInt32
        End Function

        <DllImport("CanCmd.dll", EntryPoint:="CAN_SetParam")> _
        Public Shared Function CAN_SetParam(ByVal dwDeviceHandle As UInt32, ByVal dwChannel As UInt32, ByVal RefType As UInt32, ByRef pData As Byte) As UInt32
        End Function

        <DllImport("CanCmd.dll", EntryPoint:="CAN_ReadRegister")> _
        Public Shared Function CAN_ReadRegister(ByVal dwDeviceHandle As UInt32, ByVal dwChannel As UInt32, ByVal RegAddr As UInt32, ByRef pData As Byte, ByVal len As UInt16) As UInt32
        End Function

        <DllImport("CanCmd.dll", EntryPoint:="CAN_WriteRegister")> _
        Public Shared Function CAN_WriteRegister(ByVal dwDeviceHandle As UInt32, ByVal dwChannel As UInt32, ByVal RegAddr As UInt32, ByRef pData As Byte, ByVal len As UInt16) As UInt32
        End Function

        <DllImport("CanCmd.dll", EntryPoint:="CAN_ChannelSend")> _
        Public Shared Function CAN_ChannelSend(ByVal dwDeviceHandle As UInt32, ByVal dwChannel As UInt32, ByRef pSend As CAN_DataFrame, ByVal Len As UInt32) As UInt32
        End Function

        <DllImport("CanCmd.dll", EntryPoint:="CAN_GetReceiveCount")> _
        Public Shared Function CAN_GetReceiveCount(ByVal dwDeviceHandle As UInt32, ByVal dwChannel As UInt32) As UInt32
        End Function

        <DllImport("CanCmd.dll", EntryPoint:="CAN_ClearReceiveBuffer")> _
        Public Shared Function CAN_ClearReceiveBuffer(ByVal dwDeviceHandle As UInt32, ByVal dwChannel As UInt32) As UInt32
        End Function

        <DllImport("CanCmd.dll", EntryPoint:="CAN_ChannelReceive")> _
        Public Shared Function CAN_ChannelReceive(ByVal dwDeviceHandle As UInt32, ByVal dwChannel As UInt32, ByRef pReceive As CAN_DataFrame, ByVal Len As UInt32, ByVal WaitTime As Int32) As UInt32
        End Function

        <DllImport("CanCmd.dll", EntryPoint:="CAN_GetErrorInfo")> _
        Public Shared Function CAN_GetErrorInfo(ByVal dwDeviceHandle As UInt32, ByVal dwChannel As UInt32, ByRef pErrInfo As CAN_ErrorInformation) As UInt32
        End Function

#End Region
     End Class
#End Region
End Namespace
