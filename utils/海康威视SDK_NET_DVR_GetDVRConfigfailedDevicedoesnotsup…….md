### 问题记录： 

基于海康的sdk写了一些代码，用于控制摄像头。其中有一段程序主要是用来获取NVR通道配置信息  
参考：https://blog.51cto.com/u\_15127677/4382288  
代码如下,  
用到了一个函数`NET_DVR_GetDVRConfig`

```java
#include <iostream>
#include "HCNetSDK.h"
int main()
{
            
   
     
     
    NET_DVR_Init();
    //设置连接时间与重连时间
    NET_DVR_SetConnectTime(2000, 1);
    NET_DVR_SetReconnect(10000, true);
    // 注册设备
    LONG lUserID;
    //登录参数，包括设备地址、登录用户、密码等
    NET_DVR_USER_LOGIN_INFO struLoginInfo = {
            
   
     
      0 };
    struLoginInfo.bUseAsynLogin = 0; //同步登录方式
    strcpy(struLoginInfo.sDeviceAddress, "192.168.20.106"); //设备IP地址
    struLoginInfo.wPort = 8000; //设备服务端口
    strcpy(struLoginInfo.sUserName, "admin"); //设备登录用户名
    strcpy(struLoginInfo.sPassword, "111111hk"); //设备登录密码
    //设备信息, 输出参数
    NET_DVR_DEVICEINFO_V40 struDeviceInfoV40 = {
            
   
     
      0 };
    lUserID = NET_DVR_Login_V40(&struLoginInfo, &struDeviceInfoV40);
    if (lUserID < 0)
    {
            
   
     
     
        printf("Login failed, error code: %d\n", NET_DVR_GetLastError());
        NET_DVR_Cleanup();
        return -1;
    }
    NET_DVR_IPPARACFG_V40 ipcfg;
    DWORD bytesReturned = 0;
    ipcfg.dwSize = sizeof(NET_DVR_IPPARACFG_V40);
    int iGroupNO = 0;
    bool resCode = NET_DVR_GetDVRConfig(lUserID, NET_DVR_GET_IPPARACFG_V40, iGroupNO,  &ipcfg, sizeof(NET_DVR_IPPARACFG_V40), &bytesReturned);
    if (!resCode)
    {
            
   
     
     
        DWORD code = NET_DVR_GetLastError();
        std::cout << "NET_DVR_GetDVRConfig failed " << NET_DVR_GetErrorMsg((LONG*)(&code))  << std::endl;
        NET_DVR_Logout(lUserID);
        NET_DVR_Cleanup();
        return -1;
    }
    std::cout << "设备组 " << ipcfg.dwGroupNum << " 数字通道个数 " << ipcfg.dwDChanNum <<  " 起始通道 " << ipcfg.dwStartDChan << std::endl << std::endl;
        for (int i = 0; i < ipcfg.dwDChanNum; i++)
        {
            
   
     
            
        NET_DVR_PICCFG_V30 channelInfo;
        bytesReturned = 0;
        channelInfo.dwSize = sizeof(NET_DVR_PICCFG_V30);
        int channelNum = i + ipcfg.dwStartDChan;
        NET_DVR_GetDVRConfig(lUserID, NET_DVR_GET_PICCFG_V30, channelNum, &channelInfo,  sizeof(NET_DVR_PICCFG_V30), &bytesReturned);
        std::cout <<"通道号 "<< channelNum << "\t通道名称 " << channelInfo.sChanName;
        std::cout << "\t用户名 " << ipcfg.struIPDevInfo[i].sUserName << "\t密码 " <<  ipcfg.struIPDevInfo[i].sPassword;
        std::cout << "\t设备ID " << (int)ipcfg.struIPDevInfo[i].szDeviceID;
        std::cout << "\tip地址 " << ipcfg.struIPDevInfo[i].struIP.sIpV4 << "\t端口 " <<  ipcfg.struIPDevInfo[i].wDVRPort << std::endl;
    }
    //释放SDK资源
    NET_DVR_Logout(lUserID);
    NET_DVR_Cleanup();
    return 0;
}
```

这段代码本来运行ok，但是迁移到别的机器上时，出问题了。  
报错：`NET_DVR_GetDVRConfig failed Device does not support this function`  
提示这个设备不支持这个函数，原因在手册上面有写。  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/9d7b904b06d85b717ad66cb933f2eaec.png)  
注意这句话：  
`如果设备支持IP通道个数大于0，则可以通过远程参数配置接口NET_DVR_GetDVRConfig`  
也就意味着，你要使用这个函数，需要先检验，设备支持的IP通道个数。  
同时，手册上也给了示例程序（先检验，再调用）  
示例程序如下：  
ps：有一点想吐槽，手册上面关于`NET_DVR_GetDVRConfig`这个函数的解释，丝毫没有提到这个问题，害的我好找，找了很久才找到这里有写。既然这个函数并不是所有设备都支持的，那么规范的写法就应该先检验，再调用。

```java
#include <stdio.h>
#include <iostream>
#include "Windows.h"
#include "string.h"
#include "HCNetSDK.h"
using namespace std;
void main()
{
            
   
     
     
    int i=0;
    BYTE byIPID,byIPIDHigh;
    int iDevInfoIndex, iGroupNO, iIPCh;
    DWORD dwReturned = 0;

    //---------------------------------------
    // 初始化
    NET_DVR_Init();
    //设置连接时间与重连时间
    NET_DVR_SetConnectTime(2000, 1);
    NET_DVR_SetReconnect(10000, true);

    //---------------------------------------
    // 注册设备
    LONG lUserID;

    //登录参数，包括设备地址、登录用户、密码等
    NET_DVR_USER_LOGIN_INFO struLoginInfo = {
            
   
     
     0};
    struLoginInfo.bUseAsynLogin = 0; //同步登录方式
    strcpy(struLoginInfo.sDeviceAddress, "192.0.0.64"); //设备IP地址
    struLoginInfo.wPort = 8000; //设备服务端口
    strcpy(struLoginInfo.sUserName, "admin"); //设备登录用户名
    strcpy(struLoginInfo.sPassword, "abcd1234"); //设备登录密码

    //设备信息, 输出参数
    NET_DVR_DEVICEINFO_V40 struDeviceInfoV40 = {
            
   
     
     0};

    lUserID = NET_DVR_Login_V40(&struLoginInfo, &struDeviceInfoV40);
    if (lUserID < 0)
    {
            
   
     
     
        printf("Login failed, error code: %d\n", NET_DVR_GetLastError());
        NET_DVR_Cleanup();
        return;
    }

    printf("The max number of analog channels: %d\n",struDeviceInfoV40.struDeviceV30.byChanNum); //模拟通道个数
    printf("The max number of IP channels: %d\n", struDeviceInfoV40.struDeviceV30.byIPChanNum + struDeviceInfoV40.struDeviceV30.byHighDChanNum * 256);//IP通道个数

    //获取IP通道参数信息
    NET_DVR_IPPARACFG_V40 IPAccessCfgV40;
    memset(&IPAccessCfgV40, 0, sizeof(NET_DVR_IPPARACFG));
    iGroupNO=0;
    if (!NET_DVR_GetDVRConfig(lUserID, NET_DVR_GET_IPPARACFG_V40, iGroupNO, &IPAccessCfgV40, sizeof(NET_DVR_IPPARACFG_V40), &dwReturned))
    {
            
   
     
     
        printf("NET_DVR_GET_IPPARACFG_V40 error, %d\n", NET_DVR_GetLastError());
        NET_DVR_Logout(lUserID);
        NET_DVR_Cleanup();
        return;
    }
    else
    {
            
   
     
     
        for (i=0;i<IPAccessCfgV40.dwDChanNum;i++)
        {
            
   
     
     
            switch(IPAccessCfgV40.struStreamMode[i].byGetStreamType)
            {
            
   
     
     
            case 0: //直接从设备取流
                if (IPAccessCfgV40.struStreamMode[i].uGetStream.struChanInfo.byEnable)
                {
            
   
     
     
                    byIPID=IPAccessCfgV40.struStreamMode[i].uGetStream.struChanInfo.byIPID;
                    byIPIDHigh=IPAccessCfgV40.struStreamMode[i].uGetStream.struChanInfo.byIPIDHigh;
                    iDevInfoIndex=byIPIDHigh*256 + byIPID-1-iGroupNO*64;
                    printf("IP channel no.%d is online, IP: %s\n", i+1, IPAccessCfgV40.struIPDevInfo[iDevInfoIndex].struIP.sIpV4);
                }
                break;
            case 1: //从流媒体取流
                if (IPAccessCfgV40.struStreamMode[i].uGetStream.struPUStream.struStreamMediaSvrCfg.byValid)
                {
            
   
     
     
                    printf("IP channel %d connected with the IP device by stream server.\n", i+1);
                    printf("IP of stream server: %s, IP of IP device: %s\n",IPAccessCfgV40.struStreamMode[i].uGetStream.\
                    struPUStream.struStreamMediaSvrCfg.struDevIP.sIpV4, IPAccessCfgV40.struStreamMode[i].uGetStream.\
                    struPUStream.struDevChanInfo.struIP.sIpV4);
                }
                break;
            default:
                break;
            }
        }
    }

    //配置IP通道5;
    iIPCh=4;

    //支持自定义协议
    NET_DVR_CUSTOM_PROTOCAL struCustomPro;
    if (!NET_DVR_GetDVRConfig(lUserID, NET_DVR_GET_CUSTOM_PRO_CFG, 1, &struCustomPro, sizeof(NET_DVR_CUSTOM_PROTOCAL), &dwReturned))
    //获取自定义协议1
    {
            
   
     
     
        printf("NET_DVR_GET_CUSTOM_PRO_CFG error, %d\n", NET_DVR_GetLastError());
        NET_DVR_Logout(lUserID);
        NET_DVR_Cleanup();
        return;
    }
    struCustomPro.dwEnabled=1;   //启用主码流
    struCustomPro.dwEnableSubStream=1; //启用子码流
    strcpy((char *)struCustomPro.sProtocalName,"Protocal_RTSP");  //自定义协议名称:Protocal_RTSP,最大16字节
    struCustomPro.byMainProType=1;    //主码流协议类型: 1- RTSP
    struCustomPro.byMainTransType=2;  //主码流传输协议: 0-Auto, 1-udp, 2-rtp over rtsp
    struCustomPro.wMainPort=554;     //主码流取流端口
    strcpy((char *)struCustomPro.sMainPath,"rtsp://192.168.1.65/h264/ch1/main/av_stream");//主码流取流URL
    struCustomPro.bySubProType=1;    //子码流协议类型: 1-RTSP
    struCustomPro.bySubTransType=2;  //子码流传输协议: 0-Auto, 1-udp, 2-rtp over rtsp
    struCustomPro.wSubPort=554;     //子码流取流端口
    strcpy((char *)struCustomPro.sSubPath,"rtsp://192.168.1.65/h264/ch1/sub/av_stream");//子码流取流URL

    if (!NET_DVR_SetDVRConfig(lUserID, NET_DVR_SET_CUSTOM_PRO_CFG, 1, &struCustomPro, sizeof(NET_DVR_CUSTOM_PROTOCAL)))
    //设置自定义协议1
    {
            
   
     
     
        printf("NET_DVR_SET_CUSTOM_PRO_CFG error, %d\n", NET_DVR_GetLastError());
        NET_DVR_Logout(lUserID);
        NET_DVR_Cleanup();
        return;
    }

    printf("Set the custom protocol: %s\n", "Protocal_RTSP");
    NET_DVR_IPC_PROTO_LIST m_struProtoList;
    if (!NET_DVR_GetIPCProtoList(lUserID, &m_struProtoList)) //获取设备支持的前端协议
    {
            
   
     
     
        printf("NET_DVR_GetIPCProtoList error, %d\n", NET_DVR_GetLastError());
        NET_DVR_Logout(lUserID);
        NET_DVR_Cleanup();
        return;
    }

    IPAccessCfgV40.struIPDevInfo[iIPCh].byEnable=1;     //启用
    for (i = 0; i<m_struProtoList.dwProtoNum; i++)
    {
            
   
     
     
        if(strcmp((char *)struCustomPro.sProtocalName,(char *)m_struProtoList.struProto[i].byDescribe)==0)
        {
            
   
     
     
            IPAccessCfgV40.struIPDevInfo[iIPCh].byProType=m_struProtoList.struProto[i].dwType; //选择自定义协议
            break;
        }
    }

    //IPAccessCfgV40.struIPDevInfo[iIPCh].byProType=0;  //厂家私有协议
    strcpy((char *)IPAccessCfgV40.struIPDevInfo[iIPCh].struIP.sIpV4,"192.168.1.65"); //前端IP设备的IP地址
    IPAccessCfgV40.struIPDevInfo[iIPCh].wDVRPort=8000;  //前端IP设备服务端口
    strcpy((char *)IPAccessCfgV40.struIPDevInfo[iIPCh].sUserName,"admin");  //前端IP设备登录用户名
    strcpy((char *)IPAccessCfgV40.struIPDevInfo[iIPCh].sPassword,"12345");  //前端IP设备登录密码

    IPAccessCfgV40.struStreamMode[iIPCh].byGetStreamType=0;
    IPAccessCfgV40.struStreamMode[iIPCh].uGetStream.struChanInfo.byChannel=1;
    IPAccessCfgV40.struStreamMode[iIPCh].uGetStream.struChanInfo.byIPID=(iIPCh+1)%256;
    IPAccessCfgV40.struStreamMode[iIPCh].uGetStream.struChanInfo.byIPIDHigh=(iIPCh+1)/256;

    //IP通道配置，包括添加、删除、修改IP通道等
    if (!NET_DVR_SetDVRConfig(lUserID, NET_DVR_SET_IPPARACFG_V40, iGroupNO, &IPAccessCfgV40, sizeof(NET_DVR_IPPARACFG_V40)))
    {
            
   
     
     
        printf("NET_DVR_SET_IPPARACFG_V40 error, %d\n", NET_DVR_GetLastError());
        NET_DVR_Logout(lUserID);
        NET_DVR_Cleanup();
        return;
    }
    else
    {
            
   
     
     
        printf("Set IP channel no.%d, IP: %s\n", iIPCh+1, IPAccessCfgV40.struIPDevInfo[iIPCh].struIP.sIpV4);
    }

    //注销用户
    NET_DVR_Logout(lUserID);

    //释放SDK资源
    NET_DVR_Cleanup();
    return;
}
```