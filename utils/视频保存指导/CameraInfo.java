package com.hywx.hs.video;

import com.sun.jna.NativeLong;
import lombok.Data;

@Data
public class CameraInfo {

  	private String address;
    private String userName;
    private String pwd;
    private short port;
    private NativeLong userId;
    private NativeLong channel;
	private NativeLong key;


}
