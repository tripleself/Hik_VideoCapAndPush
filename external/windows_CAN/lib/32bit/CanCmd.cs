//  CanCmd.cs
//
//  ~~~~~~~~~~~~
//
// CanCmd API
//
//  ~~~~~~~~~~~~
//
//  ------------------------------------------------------------------
//  Author : zsj
//	Last change: 12.06.2018 Wagner
//
//  Language: C# 1.0
//  ------------------------------------------------------------------
//
//  Copyright (C) 2009-2018  Nanjing LAIKE Electronic Technology Co., Ltd.
//  more Info at http://www.njlike.com
//
using System;
using System.Text;
using System.Runtime.InteropServices;

namespace Laike.Can.CanCmd {
#region Enumerations
    [Flags]
    public enum CAN_ErrorCode : uint {
        CAN_E_NOERROR               = 0x0000,   // 没有发现错误
        CAN_E_OVERFLOW              = 0x0001,   // CAN控制器内部FIFO溢出
        CAN_E_ERRORALARM            = 0x0002,   // CAN控制器错误报警
        CAN_E_PASSIVE               = 0x0004,   // CAN控制器消极错误
        CAN_E_LOSE                  = 0x0008,   // CAN控制器仲裁丢失
        CAN_E_BUSERROR              = 0x0010,   // CAN控制器总线错误

        CAN_E_DEVICEOPENED          = 0x0100,       // 设备已经打开
        CAN_E_DEVICEOPEN            = 0x0200,       // 打开设备错误
        CAN_E_DEVICENOTOPEN         = 0x0400,       // 设备没有打开
        CAN_E_BUFFEROVERFLOW        = 0x0800,       // 缓冲区溢出
        CAN_E_DEVICENOTEXIST        = 0x1000,       // 此设备不存在
        CAN_E_LOADKERNELDLL         = 0x2000,       // 装载动态库失败
        CAN_E_CMDFAILED             = 0x4000,       // 执行命令失败错误码
        CAN_E_BUFFERCREATE          = 0x8000,       // 内存不足
    }
#endregion

#region Structures
    //1.njlikeCAN系列接口卡信息的数据类型。
    [StructLayout(LayoutKind.Sequential)]
    public struct CAN_DeviceInformation {
        public UInt16 uHardWareVersion;
        public UInt16 uFirmWareVersion;
        public UInt16 uDriverVersion;
        public UInt16 uInterfaceVersion;
        public UInt16 uInterruptNumber;
        public byte bChannelNumber;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 20)]
        public byte[] szSerialNumber;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 40)]
        public byte[] szHardWareType;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 20)]
        public byte[] szDescription;
    }

//2.定义CAN信息帧的数据类型。
    unsafe public struct CAN_DataFrame {  //使用不安全代码
        public uint uTimeFlag;
        public byte nSendType;
        public byte bRemoteFlag; //是否是远程帧
        public byte bExternFlag; //是否是扩展帧
        public byte nDataLen;
        public uint uID;
        public fixed byte arryData[8];
    }

//4.定义错误信息的数据类型。
    [StructLayout(LayoutKind.Sequential)]
    public struct CAN_ErrorInformation {
        public CAN_ErrorCode uErrorCode;
        public byte PassiveErrData1;
        public byte PassiveErrData2;
        public byte PassiveErrData3;
        public byte ArLostErrData;
    }

//5.定义初始化CAN的数据类型
    [StructLayout(LayoutKind.Sequential)]
    public struct CAN_InitConfig {
        public byte bMode;                                         // 工作模式(0表示正常模式,1表示只听模式)///< 工作模式(0表示正常模式,1表示只听模式)
        public byte nBtrType;                                      // 位定时参数模式(1表示SJA1000,0表示LPC21XX)// 位定时参数模式(1表示SJA1000,0表示LPC21XX)
        //[MarshalAs(UnmanagedType.ByValArray, SizeConst = 4)]       // CAN位定时参数
        public byte  dwBtr0;
        public byte  dwBtr1;
        public byte  dwBtr2;
        public byte  dwBtr3;
        public UInt32 dwAccCode;                                   // 验收码
        public UInt32 dwAccMask;                                   // 屏蔽码
        public byte nFilter;                                       // 滤波方式(0表示未设置滤波功能,1表示双滤波,2表示单滤波)
        public byte dwReserved;                                    // 预留字段
    }
#endregion

#region CanCmd class
    public static class CanCmd {
#region Parameter values definition
        public const UInt32 CAN_RESULT_OK    = 1;
        public const UInt32 CAN_RESULT_ERROR = 0;

        // CAN 卡类型定义
        public const UInt32 LCUSB_131B      = 1;
        public const UInt32 LCUSB_132B      = 2;
        public const UInt32 LCPCI_252       = 4;
        public const UInt32 LCMiniPcie_431  = 1;
        public const UInt32 LCMiniPcie_432  = 2;
        public const UInt32 USBCAN_1CH      = 13;
        public const UInt32 USBCAN_C_1CH    = 14;
        public const UInt32 USBCAN_E_1CH    = 15;
        public const UInt32 USBCAN_E_2CH    = 16;
        public const UInt32 MPCIeCAN_1CH    = 17;
        public const UInt32 MPCIeCAN_2CH    = 18;
#endregion

#region CanCmd API Implementation

        [DllImport("CanCmd.dll")]
        public static extern UInt32 CAN_DeviceOpen(UInt32 dwType, UInt32 dwIndex, ref char pDescription);
        [DllImport("CanCmd.dll")]
        public static extern UInt32 CAN_DeviceClose(UInt32 dwDeviceHandle);
        [DllImport("CanCmd.dll")]
        public static extern UInt32 CAN_ChannelStart(UInt32 dwDeviceHandle, UInt32 dwChannel, ref CAN_InitConfig pInitConfig);
        [DllImport("CanCmd.dll")]
        public static extern UInt32 CAN_ChannelStop(UInt32 dwDeviceHandle, UInt32 dwChannel);

        [DllImport("CanCmd.dll")]
        public static extern UInt32 CAN_GetDeviceInfo(UInt32 dwDeviceHandle, ref CAN_DeviceInformation pInfo);

        [DllImport("CanCmd.dll")]
        public static extern UInt32 CAN_GetParam(UInt32 dwDeviceHandle, UInt32 dwChannel, UInt32 RefType, ref byte pData);
        [DllImport("CanCmd.dll")]
        public static extern UInt32 CAN_SetParam(UInt32 dwDeviceHandle, UInt32 dwChannel, UInt32 RefType, ref byte pData);

        [DllImport("CanCmd.dll")]
        public static extern UInt32 CAN_ReadRegister(UInt32 dwDeviceHandle, UInt32 dwChannel, UInt32 RegAddr, ref byte pData, UInt16 len);
        [DllImport("CanCmd.dll")]
        public static extern UInt32 CAN_WriteRegister(UInt32 dwDeviceHandle, UInt32 dwChannel, UInt32 RegAddr, ref byte pData, UInt16 len);

        [DllImport("CanCmd.dll")]
        public static extern UInt32 CAN_ChannelSend(UInt32 dwDeviceHandle, UInt32 dwChannel, ref CAN_DataFrame pSend, UInt32 Len);

        [DllImport("CanCmd.dll")]
        public static extern UInt32 CAN_GetReceiveCount(UInt32 dwDeviceHandle, UInt32 dwChannel);
        [DllImport("CanCmd.dll")]
        public static extern UInt32 CAN_ClearReceiveBuffer(UInt32 dwDeviceHandle, UInt32 dwChannel);
        
        [DllImport("CanCmd.dll", CharSet = CharSet.Ansi)]
        public static extern UInt32 CAN_ChannelReceive(UInt32 dwDeviceHandle, UInt32 dwChannel, IntPtr pReceive, UInt32 Len, Int32 WaitTime);

        [DllImport("CanCmd.dll")]
        public static extern UInt32 CAN_GetErrorInfo(UInt32 dwDeviceHandle, UInt32 dwChannel, ref CAN_ErrorInformation pErrInfo);

#endregion
    }
#endregion
}
