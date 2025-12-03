package com.hywx.hs.video;

import cn.novelweb.video.edit.VideoEditing;
import cn.novelweb.video.pojo.ProgramConfig;
import com.hywx.hs.entity.tables.CameraBasic;
import com.hywx.hs.enums.VideoStatusEnums;
import com.hywx.hs.service.CameraBasicService;
import com.hywx.hs.utils.DateUtil;
import com.sun.jna.NativeLong;
import com.sun.jna.Pointer;
import com.sun.jna.ptr.ByteByReference;
import lombok.extern.slf4j.Slf4j;

import java.io.File;

@Slf4j
public class HIKSDKSaveRealData {

    private HCNetSDK sdk = HCNetSDK.INSTANCE;

    static{
        ProgramConfig programConfig = new ProgramConfig();
        programConfig.setPath("sdk/ffmpeg/bin/ffmpeg");
        programConfig.setDeBugLog(false);
        VideoEditing.init(programConfig);
    }

    /*****************************************************************
     * 注册设备并返回参数
     * 目的:   注册设备并返回key    :Nativelong
     * parameters:    cameraInfo
     *return:        cameraInfo
     * ****************************************************************/
    public CameraInfo loginHIK(String address,String userName,String pwd,short port){
        CameraInfo cameraInfo = new CameraInfo();
        cameraInfo.setAddress(address);
        cameraInfo.setUserName(userName);
        cameraInfo.setPwd(pwd);
        cameraInfo.setPort(port);
        //设置设备通道号 通道号为1
        NativeLong channel = new NativeLong(1);
        cameraInfo.setChannel(channel);
        if (!sdk.NET_DVR_Init()) {
            log.error("初始化失败..................");
        }
        //创建设备
        HCNetSDK.NET_DVR_DEVICEINFO_V30 deInfo = new HCNetSDK.NET_DVR_DEVICEINFO_V30();
        //注册用户设备
        NativeLong id = sdk.NET_DVR_Login_V30(cameraInfo.getAddress(), cameraInfo.getPort(), cameraInfo.getUserName(), cameraInfo.getPwd(), deInfo);
        cameraInfo.setUserId(id);
        //判断是否注册成功
        if (cameraInfo.getUserId().intValue() < 0) {
            log.error("注册设备失败 错误码为: " + sdk.NET_DVR_GetLastError());
        }
        //判断是否获取到设备能力
        HCNetSDK.NET_DVR_WORKSTATE_V30 devWork = new HCNetSDK.NET_DVR_WORKSTATE_V30();
        if (!sdk.NET_DVR_GetDVRWorkState_V30(cameraInfo.getUserId(), devWork)) {
            log.error("获取设备能力集失败,返回设备状态失败...............");
        }
        //启动实时预览功能  创建clientInfo对象赋值预览参数
        HCNetSDK.NET_DVR_CLIENTINFO clientInfo = new HCNetSDK.NET_DVR_CLIENTINFO();

        clientInfo.lChannel = cameraInfo.getChannel();   //设置通道号
        clientInfo.lLinkMode = new NativeLong(0);  //TCP取流
        clientInfo.sMultiCastIP = null;                   //不启动多播模式

        //创建窗口句柄
        clientInfo.hPlayWnd = null;

        FRealDataCallBack fRealDataCallBack = new FRealDataCallBack();
        //开启实时预览
        NativeLong key = sdk.NET_DVR_RealPlay_V30(cameraInfo.getUserId(), clientInfo, fRealDataCallBack, null, true);
        cameraInfo.setKey(key);
        //判断是否预览成功
        if (key.intValue() == -1) {
            log.error("预览失败   错误代码为:  " + sdk.NET_DVR_GetLastError());
            logoutHIK(cameraInfo);
        }
        return cameraInfo;
    }


    public void saveRealData(CameraBasicService cameraBasicService, CameraInfo cameraInfo, String localSaveFilePath,Integer cameraId){
        try {
            while(true){
                CameraBasic cameraBasic = cameraBasicService.getById(cameraId);
                Integer cameraStatus = cameraBasic.getSwitchStatus();
                short lengthOfTime = (short) (cameraBasic.getCycle() * 60);
                if (VideoStatusEnums.OPEN.getCode().equals(cameraStatus)){
                    String fileName = DateUtil.getCurrentDate();
                    log.info("设备{}开始录制...,文件{}录制开始",cameraId,fileName);
                    PTZ(cameraInfo,lengthOfTime,localSaveFilePath,fileName);
                    log.info("设备{}录制结束！文件{}录制结束",cameraId,fileName);
                }else {
                    log.info("设备{}已关闭！",cameraBasic.getCameraName());
                    break;
                }
            }
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    public void saveRealData(CameraInfo cameraInfo, short lengthOfTime,String localSaveFilePath,String fileName){
        try {
            //log.info("设备{}开始录制...,文件{}录制开始");
            PTZ(cameraInfo,lengthOfTime,localSaveFilePath,fileName);
            //log.info("设备{}录制结束！文件{}录制结束");
        } catch (Exception e) {
            e.printStackTrace();
        }

    }

    public void logoutHIK(CameraInfo cameraInfo){
        sdk.NET_DVR_StopRealPlay(cameraInfo.getKey());
        sdk.NET_DVR_Logout(cameraInfo.getUserId());
        sdk.NET_DVR_Cleanup();
        System.exit(0);
    }


    private void PTZ(CameraInfo cameraInfo, short lengthOfTime ,String localSaveFilePath,String fileName) throws Exception {
        // 查看文件夹是否存在,如果不存在则创建
        File file = new File(localSaveFilePath);
        if (!file.exists()) {
            file.mkdir();
        }
        HCNetSDK.NET_DVR_CLIENTINFO clientInfo = new HCNetSDK.NET_DVR_CLIENTINFO();
        clientInfo.lChannel = cameraInfo.getChannel();   //设置通道号
        clientInfo.lLinkMode = new NativeLong(0);  //TCP取流
        clientInfo.sMultiCastIP = null;                   //不启动多播模式
        //创建窗口句柄
        clientInfo.hPlayWnd = null;
        HCNetSDK.NET_DVR_JPEGPARA netDvrJpegpara = new HCNetSDK.NET_DVR_JPEGPARA();
        netDvrJpegpara.wPicQuality = 2;
        netDvrJpegpara.wPicSize =2;
        if(!sdk.NET_DVR_CaptureJPEGPicture(cameraInfo.getUserId(),cameraInfo.getChannel(),netDvrJpegpara , file.getPath() + "/" + fileName + ".jpg")){
            log.error("保存预览图到文件夹失败 错误码为:  " + sdk.NET_DVR_GetLastError());
        }

        HCNetSDK.NET_DVR_I_FRAME netDvrIFrame = new HCNetSDK.NET_DVR_I_FRAME();
        netDvrIFrame.read();
        netDvrIFrame.dwChannel = 1;
        netDvrIFrame.byStreamType = 0;
        netDvrIFrame.dwSize = netDvrIFrame.size();
        netDvrIFrame.write();

        if(!sdk.NET_DVR_RemoteControl(cameraInfo.getUserId(),3402,netDvrIFrame.getPointer(),netDvrIFrame.dwSize)){
            log.error("强制I帧 错误码为:  " + sdk.NET_DVR_GetLastError());
        }
        //预览成功后 调用接口使视频资源保存到文件中
        if (!sdk.NET_DVR_SaveRealData_V30(cameraInfo.getKey(), 2,file.getPath() + "/" + fileName + ".mp4")) {
            log.error("保存视频文件到文件夹失败 错误码为:  " + sdk.NET_DVR_GetLastError());
            logoutHIK(cameraInfo);
            return;
        }
        Thread.sleep(lengthOfTime*1000);
        sdk.NET_DVR_StopSaveRealData(cameraInfo.getKey());

        //格式转化
//        VideoEditing.converterToMp4(file.getPath()+ "\\"+ tmpName + ".mp4",file.getPath()+"\\" + fileName + ".mp4", null);
//        File delFile = new File(file.getPath() + "\\" + tmpName + ".mp4");
//        if (delFile.isFile() && delFile.exists()) {
//            delFile.delete();
//        }
        //File[] listFile = file.listFiles();


    }


    class FRealDataCallBack implements HCNetSDK.FRealDataCallBack_V30 {
        //预览回调
        public void invoke(NativeLong lRealHandle, int dwDataType, ByteByReference pBuffer, int dwBufSize, Pointer pUser) {

        }
    }


    public static void main(String[] args) {
        HIKSDKSaveRealData hiksdkSaveRealData = new HIKSDKSaveRealData();
        CameraInfo cameraInfo = hiksdkSaveRealData.loginHIK("192.168.0.182","admin","1234.com",(short)8000);
        for(int i =0;i<10;i++){
            String fileName = DateUtil.getCurrentDate();
            hiksdkSaveRealData.saveRealData(cameraInfo,(short)10,"D:/realData",fileName);
        }

    }



}
