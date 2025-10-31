Attribute VB_Name = "CanCmd"
'������Ҫ�õ������ݽṹ

'//CAN����֡����
Public Type PCAN_DataFrame
    uTimeFlag       As Long                     ' // ʱ���ʶ,�Խ���֡��Ч
    nSendType       As Byte                     ' // ����֡����,0-��������;1-���η���;2-�Է�����;3-�����Է�����
    bRemoteFlag     As Byte                     ' // �Ƿ���Զ��֡
    bExternFlag     As Byte                     ' // �Ƿ�����չ֡
    nDataLen        As Byte                     ' // ���ݳ���
    uID             As Long                     ' // ����DI
    arryData(7)     As Byte                     ' // ��������
End Type

'//CAN��ʼ������
Public Type PCAN_InitConfig
    bMode           As Byte                     '// ����ģʽ(0��ʾ����ģʽ,1��ʾֻ��ģʽ)
    nBtrType        As Byte '                   // λ��ʱ����ģʽ(1��ʾSJA1000,0��ʾLPC21XX)
    dwBtr(3)        As Byte '                   // CANλ��ʱ����
    dwAccCode       As Long '                   // ������
    dwAccMask       As Long '                   // ������
    nFilter         As Byte '                   // �˲���ʽ(0��ʾδ�����˲�����,1��ʾ˫�˲�,2��ʾ���˲�
    dwReserved      As Byte '                   // Ԥ���ֶ�
End Type

'//CAN�豸��Ϣ
Public Type PCAN_DeviceInformation
    uHardWareVersion    As Integer '            // Ӳ���汾
    uFirmWareVersion    As Integer '            // �̼��汾
    uDriverVersion      As Integer '            // �����汾
    uInterfaceVersion   As Integer '            // �ӿڿ�汾
    uInterruptNumber    As Integer '            // �жϺ�
    bChannelNumber      As Byte '               // �м�·CAN
    szSerialNumber(19)  As Byte '               // �豸���к�
    szHardWareType(39)  As Byte '               // Ӳ������
    szDescription(19)   As Byte '               // �豸����
End Type


'//CAN������Ϣ
Public Type PCAN_ErrorInformation
    uErrorCode      As Long '                   // ��������
    PassiveErrData(3)   As Byte '               // ������������
    ArLostErrData   As Byte '                   // �ٲô�������
End Type

'// ���豸
Declare Function CAN_DeviceOpen Lib "CanCmd.dll" (ByVal dwType As Long, ByVal dwIndex As Long, ByVal pDescription As Long) As Long

'// �ر��豸
Declare Function CAN_DeviceClose Lib "CanCmd.dll" (ByVal dwDeviceHandle As Long) As Long

'// ����CAN
Declare Function CAN_ChannelStart Lib "CanCmd.dll" (ByVal dwDeviceHandle As Long, ByVal dwChannel As Long, ByRef pInitConfig As PCAN_InitConfig) As Long

'// ֹͣCAN
Declare Function CAN_ChannelStop Lib "CanCmd.dll" (ByVal dwDeviceHandle As Long, ByVal dwChannel As Long) As Long

'// ��ȡ�豸��Ϣ
Declare Function CAN_GetDeviceInfo Lib "CanCmd.dll" (ByVal dwDeviceHandle As Long, ByRef pInfo As PCAN_DeviceInformation) As Long

'// ��ȡCAN������Ϣ
Declare Function CAN_GetErrorInfo Lib "CanCmd.dll" (ByVal dwDeviceHandle As Long, ByVal dwChannel As Long, ByRef pErr As PCAN_ErrorInformation) As Long

'// ��������
Declare Function CAN_ChannelSend Lib "CanCmd.dll" (ByVal dwDeviceHandle As Long, ByVal dwChannel As Long, ByRef pSend As PCAN_DataFrame, ByVal nCount As Long) As Long

'// �ӽ��ջ������ж�����
' nWaitTime �ڵ���ǰ��ֵΪ -1��
Declare Function CAN_ChannelReceive Lib "CanCmd.dll" (ByVal dwDeviceHandle As Long, ByVal dwChannel As Long, ByRef pReceive As PCAN_DataFrame, ByVal nCount As Long, ByVal nWaitTime As Integer) As Long

'// ��ȡ���ջ�����֡��
Declare Function CAN_GetReceiveCount Lib "CanCmd.dll" (ByVal dwDeviceHandle As Long, ByVal dwChannel As Long) As Long

'// ��ս��ջ�����
Declare Function CAN_ClearReceiveBuffer Lib "CanCmd.dll" (ByVal dwDeviceHandle As Long, ByVal dwChannel As Long) As Long

'// ���Ĵ���
Declare Function CAN_ReadRegister Lib "CanCmd.dll" (ByVal dwDeviceHandle As Long, ByVal dwChannel As Long, ByVal dwAddr As Long, ByRef pBuff As Byte, ByVal nLen As Integer) As Long

'// д�Ĵ���
Declare Function CAN_WriteRegister Lib "CanCmd.dll" (ByVal dwDeviceHandle As Long, ByVal dwChannel As Long, ByVal dwAddr As Long, ByRef pBuff As Byte, ByVal nLen As Integer) As Long

'// ��ȡ����
Declare Function CAN_GetParam Lib "CanCmd.dll" (ByVal dwDeviceHandle As Long, ByVal dwChannel As Long, ByVal dwParamType As Long, ByRef pData As Byte) As Long

'// ���ò���
Declare Function CAN_SetParam Lib "CanCmd.dll" (ByVal dwDeviceHandle As Long, ByVal dwChannel As Long, ByVal dwParamType As Long, ByRef pData As Byte) As Long
