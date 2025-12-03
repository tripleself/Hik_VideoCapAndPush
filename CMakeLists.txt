cmake_minimum_required(VERSION 3.18)
project(Identify)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# 启用CUDA支持
enable_language(CUDA)

# 查找OpenCV
set(OpenCV_DIR "D:/dev_libs/opencv/build/x64/vc16/lib")
find_package(OpenCV REQUIRED)

# 查找CUDA
find_package(CUDAToolkit REQUIRED)

# 定义其他库路径
set(HIKVISION_SDK_DIR "${CMAKE_SOURCE_DIR}/external/CH-HCNetSDKV6.1.9.48")
set(FFMPEG_DIR "${CMAKE_SOURCE_DIR}/external/ffmpeg")
set(NLOHMANN_DIR "${CMAKE_SOURCE_DIR}/external/nlohmann")
set(CANCMD_DIR "${CMAKE_SOURCE_DIR}/external/windows_CAN")
set(YOLO_TRACK_DIR "${CMAKE_SOURCE_DIR}/external/yolo_track")
set(TENSORRT_DIR "D:/Program/TensorRT-8.6.1.6")
set(EIGEN3_DIR "D:/dev_libs/eigen-3.3.8")

# 包含目录
include_directories(
    ${CMAKE_SOURCE_DIR}/head
    ${OpenCV_INCLUDE_DIRS}
    ${EIGEN3_INCLUDE_DIR}
    ${HIKVISION_SDK_DIR}/include
    ${FFMPEG_DIR}/include
    ${NLOHMANN_DIR}/include
    ${CANCMD_DIR}/lib/64bit
    ${YOLO_TRACK_DIR}/include
    ${TENSORRT_DIR}/include
    ${EIGEN3_DIR}
)

# 链接库目录
link_directories(
    ${HIKVISION_SDK_DIR}/lib
    ${FFMPEG_DIR}/lib
    ${CANCMD_DIR}/lib/64bit
    ${OpenCV_DIR}/x64/vc17/lib
    ${YOLO_TRACK_DIR}/lib/Release
    ${TENSORRT_DIR}/lib
)

# 源文件
set(SOURCES
    src/main.cpp
    src/TaskDisplay.cpp
    src/TaskLocating.cpp
    src/TaskRTSPStream.cpp
    src/TaskVideoCapture.cpp
    src/TaskThermalCapture.cpp
    src/TaskObjectTracking.cpp
    src/LocationReporter.cpp
    src/TaskLocationReporter.cpp
    src/ThreadManager.cpp
    src/ControlServer.cpp
)

# CUDA源文件
set(CUDA_SOURCES
    src/PushStream.cu
)

# 创建主程序可执行文件
add_executable(${PROJECT_NAME} ${SOURCES} ${CUDA_SOURCES})

# 链接库 - 主程序
target_link_libraries(${PROJECT_NAME}
    ${OpenCV_LIBS}
    CUDA::cudart
    CUDA::cublas
    HCNetSDK
    PlayCtrl
    HCCore
    GdiPlus
    CanCmd
    avcodec
    avformat
    avutil
    swscale
    swresample
    avfilter
    postproc
    avdevice
    yolo_infer
    bytetrack
    counting_line
    ws2_32
)
