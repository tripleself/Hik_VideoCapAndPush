#include "TaskRTSPStream.h"
#include <iostream>
#include "PushStream.cuh"
#include <thread>
#include <chrono>

#ifdef _WIN32
#define POPEN _popen
#define PCLOSE _pclose
#else
#define POPEN popen
#define PCLOSE pclose
#endif

// 构造函数，初始化RTSP流对象和共享数据
TaskRTSPStream::TaskRTSPStream(SharedData &data, const std::vector<std::string> &rtspUrls, int streamWidth, int streamHeight, int streamFps)
    : data_(data), rtspUrls_(rtspUrls), streamWidth_(streamWidth), streamHeight_(streamHeight), streamFps_(streamFps)
{
}

// 析构函数，释放 RTSP 流对象
TaskRTSPStream::~TaskRTSPStream()
{
    stop();
}

// 启动视频流传输线程
void TaskRTSPStream::start()
{
    thread_ = std::thread(&TaskRTSPStream::run, this);
}

// 停止视频流传输线程
void TaskRTSPStream::stop()
{
    data_.isRunning = false;
    if (thread_.joinable())
    {
        thread_.join();
    }
    writer_.release();
}

void TaskRTSPStream::run()
{
    cv::Mat frameT1, frameT2, frameV1, frameV2;
    int frameWidth = 0, frameHeight = 0;
    int fps = streamFps_;

    // 等待任意设备的视频数据就绪（支持双设备）
    std::cout << "[TaskRTSPStream] 等待视频数据就绪..." << std::endl;
    bool dataReady = false;
    while (!dataReady && data_.isRunning)
    {
        bool device1Ready = false;
        bool device2Ready = false;

        // 检查设备1数据（分别加锁避免死锁）
        {
            std::lock_guard<std::mutex> lock1(data_.processed_thermal_mutex_1);
            device1Ready = !data_.processed_thermal_frame_1.empty();
        }
        if (!device1Ready)
        {
            std::lock_guard<std::mutex> lock2(data_.processed_visible_mutex_1);
            device1Ready = !data_.processed_visible_frame_1.empty();
        }

        // 检查设备2数据（分别加锁避免死锁）
        {
            std::lock_guard<std::mutex> lock3(data_.processed_thermal_mutex_2);
            device2Ready = !data_.processed_thermal_frame_2.empty();
        }
        if (!device2Ready)
        {
            std::lock_guard<std::mutex> lock4(data_.processed_visible_mutex_2);
            device2Ready = !data_.processed_visible_frame_2.empty();
        }

        dataReady = device1Ready || device2Ready;
        if (!dataReady)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    // 获取帧尺寸（优先从设备1，如果没有则从设备2）
    cv::Mat referenceFrame;
    while (data_.isRunning && referenceFrame.empty())
    {
        // 尝试从设备1获取参考帧
        {
            std::lock_guard<std::mutex> lock(data_.processed_thermal_mutex_1);
            if (!data_.processed_thermal_frame_1.empty())
            {
                data_.processed_thermal_frame_1.copyTo(referenceFrame);
                std::cout << "[TaskRTSPStream] 使用设备1热成像帧作为尺寸参考" << std::endl;
            }
        }

        if (referenceFrame.empty())
        {
            std::lock_guard<std::mutex> lock(data_.processed_visible_mutex_1);
            if (!data_.processed_visible_frame_1.empty())
            {
                data_.processed_visible_frame_1.copyTo(referenceFrame);
                std::cout << "[TaskRTSPStream] 使用设备1可见光帧作为尺寸参考" << std::endl;
            }
        }

        // 如果设备1没有数据，尝试从设备2获取
        if (referenceFrame.empty())
        {
            std::lock_guard<std::mutex> lock(data_.processed_thermal_mutex_2);
            if (!data_.processed_thermal_frame_2.empty())
            {
                data_.processed_thermal_frame_2.copyTo(referenceFrame);
                std::cout << "[TaskRTSPStream] 使用设备2热成像帧作为尺寸参考" << std::endl;
            }
        }

        if (referenceFrame.empty())
        {
            std::lock_guard<std::mutex> lock(data_.processed_visible_mutex_2);
            if (!data_.processed_visible_frame_2.empty())
            {
                data_.processed_visible_frame_2.copyTo(referenceFrame);
                std::cout << "[TaskRTSPStream] 使用设备2可见光帧作为尺寸参考" << std::endl;
            }
        }

        if (referenceFrame.empty())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    // 确定最终推流分辨率
    if (streamWidth_ > 0 && streamHeight_ > 0)
    {
        // 使用配置的分辨率
        frameWidth = streamWidth_;
        frameHeight = streamHeight_;
        std::cout << "[TaskRTSPStream] 使用配置的分辨率: " << frameWidth << "x" << frameHeight << std::endl;
    }
    else
    {
        // 使用原始分辨率
        frameWidth = referenceFrame.cols;
        frameHeight = referenceFrame.rows;
        std::cout << "[TaskRTSPStream] 使用原始分辨率: " << frameWidth << "x" << frameHeight << std::endl;
    }

    if (frameWidth <= 0 || frameHeight <= 0)
    {
        std::cout << "[TaskRTSPStream] 无法获取有效的帧尺寸，退出推流" << std::endl;
        return;
    }

    // 创建双摄像头RTSP推流器
    auto pusherT1 = std::make_unique<FFmpegRtspPusher>(rtspUrls_[0], frameWidth, frameHeight, fps); // 设备1热成像
    auto pusherV1 = std::make_unique<FFmpegRtspPusher>(rtspUrls_[1], frameWidth, frameHeight, fps); // 设备1可见光
    auto pusherT2 = std::make_unique<FFmpegRtspPusher>(rtspUrls_[2], frameWidth, frameHeight, fps); // 设备2热成像
    auto pusherV2 = std::make_unique<FFmpegRtspPusher>(rtspUrls_[3], frameWidth, frameHeight, fps); // 设备2可见光
    std::cout << "[TaskRTSPStream]尝试打开RTSP推流器" << std::endl;
    // 尝试打开所有推流器
    bool pusher1Success = pusherT1->open() && pusherV1->open();
    bool pusher2Success = pusherT2->open() && pusherV2->open();

    if (!pusher1Success && !pusher2Success)
    {
        std::cout << "[TaskRTSPStream] 所有RTSP推流器创建失败" << std::endl;
        return;
    }
    if (pusher1Success)
        std::cout << "[TaskRTSPStream] 设备1 RTSP推流器创建成功" << std::endl;
    if (pusher2Success)
        std::cout << "[TaskRTSPStream] 设备2 RTSP推流器创建成功" << std::endl;

    while (data_.isRunning)
    {
        // ========== 处理设备1数据 ==========
        if (pusher1Success)
        {
            {
                std::lock_guard<std::mutex> lock(data_.processed_thermal_mutex_1);
                if (!data_.processed_thermal_frame_1.empty())
                    data_.processed_thermal_frame_1.copyTo(frameT1);
            }

            {
                std::lock_guard<std::mutex> lock(data_.processed_visible_mutex_1);
                if (!data_.processed_visible_frame_1.empty())
                    data_.processed_visible_frame_1.copyTo(frameV1);
            }

            if (!frameT1.empty())
            {
                // 调整热成像图像尺寸到配置分辨率
                if (frameT1.cols != frameWidth || frameT1.rows != frameHeight)
                {
                    cv::resize(frameT1, frameT1, cv::Size(frameWidth, frameHeight), 0, 0, cv::INTER_LINEAR);
                }
                pusherT1->pushFrame(frameT1);
            }

            if (!frameV1.empty())
            {
                // 调整可见光图像尺寸到配置分辨率
                if (frameV1.cols != frameWidth || frameV1.rows != frameHeight)
                {
                    cv::resize(frameV1, frameV1, cv::Size(frameWidth, frameHeight), 0, 0, cv::INTER_LINEAR);
                }
                pusherV1->pushFrame(frameV1);
            }
        }

        // ========== 处理设备2数据 ==========
        if (pusher2Success)
        {
            {
                std::lock_guard<std::mutex> lock(data_.processed_thermal_mutex_2);
                if (!data_.processed_thermal_frame_2.empty())
                    data_.processed_thermal_frame_2.copyTo(frameT2);
            }

            {
                std::lock_guard<std::mutex> lock(data_.processed_visible_mutex_2);
                if (!data_.processed_visible_frame_2.empty())
                    data_.processed_visible_frame_2.copyTo(frameV2);
            }

            if (!frameT2.empty())
            {
                // 调整热成像图像尺寸到配置分辨率
                if (frameT2.cols != frameWidth || frameT2.rows != frameHeight)
                {
                    cv::resize(frameT2, frameT2, cv::Size(frameWidth, frameHeight), 0, 0, cv::INTER_LINEAR);
                }
                pusherT2->pushFrame(frameT2);
            }

            if (!frameV2.empty())
            {
                // 调整可见光图像尺寸到配置分辨率
                if (frameV2.cols != frameWidth || frameV2.rows != frameHeight)
                {
                    cv::resize(frameV2, frameV2, cv::Size(frameWidth, frameHeight), 0, 0, cv::INTER_LINEAR);
                }
                pusherV2->pushFrame(frameV2);
            }
        }

        // 基于时间戳的精确帧率控制，移除固定延迟 [[memory:852229]]
        static auto last_frame_time = std::chrono::steady_clock::now();
        auto current_time = std::chrono::steady_clock::now();
        auto frame_duration = std::chrono::milliseconds(1000 / fps);
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - last_frame_time);

        if (elapsed < frame_duration)
        {
            std::this_thread::sleep_for(frame_duration - elapsed);
        }
        last_frame_time = std::chrono::steady_clock::now();
    }

    // 关闭所有推流器
    if (pusher1Success)
    {
        pusherT1->close();
        pusherV1->close();
        std::cout << "[TaskRTSPStream] 设备1推流器已关闭" << std::endl;
    }

    if (pusher2Success)
    {
        pusherT2->close();
        pusherV2->close();
        std::cout << "[TaskRTSPStream] 设备2推流器已关闭" << std::endl;
    }
}

// FFmpeg推流器构造函数
FFmpegRtspPusher::FFmpegRtspPusher(const std::string &rtspUrl, int width, int height, int fps)
    : rtspUrl_(rtspUrl), width_(width), height_(height), fps_(fps), frameIndex_(0),
      ofmt_ctx_(nullptr), video_st_(nullptr), codec_ctx_(nullptr), sws_ctx_(nullptr),
      clientDisconnected_(false), consecutiveErrors_(0)
{
}

// FFmpeg推流器析构函数
FFmpegRtspPusher::~FFmpegRtspPusher()
{
    close();
}

// FFmpeg推流器：打开RTSP推流
bool FFmpegRtspPusher::open()
{
    if (width_ <= 0 || height_ <= 0 || fps_ <= 0)
        return false;

    // 重置连接状态，允许重新推流
    resetConnectionState();

    try
    {
        avformat_network_init();

        // 完全屏蔽FFmpeg日志输出
        av_log_set_level(AV_LOG_QUIET);

        // 分配输出上下文
        int ret = avformat_alloc_output_context2(&ofmt_ctx_, nullptr, "rtsp", rtspUrl_.c_str());
        if (ret < 0 || !ofmt_ctx_)
        {
            char errbuf[AV_ERROR_MAX_STRING_SIZE];
            av_strerror(ret, errbuf, AV_ERROR_MAX_STRING_SIZE);
            std::cout << "[TaskRTSPStream] 分配输出上下文失败: " << errbuf << std::endl;
            return false;
        }

        // 查找H.264编码器
        std::cout << "[TaskRTSPStream] 查找H.264编码器" << std::endl;
        const AVCodec *codec = avcodec_find_encoder(AV_CODEC_ID_H264);
        if (!codec)
        {
            cleanup();
            return false;
        }

        // 分配编码器上下文
        std::cout << "[TaskRTSPStream] 分配编码器上下文" << std::endl;
        codec_ctx_ = avcodec_alloc_context3(codec);
        if (!codec_ctx_)
        {
            cleanup();
            return false;
        }

        // 配置编码器参数
        std::cout << "[TaskRTSPStream] 配置编码器参数" << std::endl;
        codec_ctx_->codec_id = AV_CODEC_ID_H264;
        codec_ctx_->codec_type = AVMEDIA_TYPE_VIDEO;
        codec_ctx_->pix_fmt = AV_PIX_FMT_YUV420P;
        codec_ctx_->width = width_;
        codec_ctx_->height = height_;
        codec_ctx_->time_base = AVRational{1, fps_};
        codec_ctx_->framerate = AVRational{fps_, 1};
        codec_ctx_->gop_size = fps_;
        codec_ctx_->max_b_frames = 0;
        codec_ctx_->bit_rate = static_cast<int64_t>(width_ * height_ * fps_ * 0.15);
        codec_ctx_->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

        // 设置H.264优化参数 [[memory:852229]]
        std::cout << "[TaskRTSPStream] 设置H.264优化参数" << std::endl;
        av_opt_set(codec_ctx_->priv_data, "preset", "ultrafast", 0); // 使用ultrafast降低延迟
        av_opt_set(codec_ctx_->priv_data, "tune", "zerolatency", 0);
        av_opt_set(codec_ctx_->priv_data, "profile", "baseline", 0);

        // FFmpeg 7.1版本兼容性：设置全局头标志
        if (ofmt_ctx_->oformat->flags & AVFMT_GLOBALHEADER)
        {
            codec_ctx_->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
        }

        // 打开编码器
        std::cout << "[TaskRTSPStream] 打开编码器" << std::endl;
        ret = avcodec_open2(codec_ctx_, codec, nullptr);
        if (ret < 0)
        {
            cleanup();
            return false;
        }

        // 创建视频流
        std::cout << "[TaskRTSPStream] 创建视频流" << std::endl;
        video_st_ = avformat_new_stream(ofmt_ctx_, nullptr);
        if (!video_st_)
        {
            cleanup();
            return false;
        }

        video_st_->id = ofmt_ctx_->nb_streams - 1;
        video_st_->time_base = codec_ctx_->time_base;

        // 复制编码器参数到流
        std::cout << "[TaskRTSPStream] 复制编码器参数到流" << std::endl;
        ret = avcodec_parameters_from_context(video_st_->codecpar, codec_ctx_);
        if (ret < 0)
        {
            cleanup();
            return false;
        }

        // 打开RTSP输出
        std::cout << "[TaskRTSPStream] 打开RTSP输出: " << rtspUrl_ << std::endl;
        if (!(ofmt_ctx_->oformat->flags & AVFMT_NOFILE))
        {
            AVDictionary *opts = nullptr;
            av_dict_set(&opts, "rtsp_transport", "tcp", 0);
            av_dict_set(&opts, "stimeout", "5000000", 0);    // 增加超时时间到5秒
            av_dict_set(&opts, "tcp_nodelay", "1", 0);       // 网络优化 [[memory:852229]]
            av_dict_set(&opts, "buffer_size", "1024000", 0); // 减少缓冲区大小

            ret = avio_open2(&ofmt_ctx_->pb, rtspUrl_.c_str(), AVIO_FLAG_WRITE, nullptr, &opts);
            av_dict_free(&opts);

            if (ret < 0)
            {
                char errbuf[AV_ERROR_MAX_STRING_SIZE];
                av_strerror(ret, errbuf, AV_ERROR_MAX_STRING_SIZE);
                std::cout << "[TaskRTSPStream] 打开RTSP输出失败: " << errbuf << std::endl;
                cleanup();
                return false;
            }
        }

        // 写入流头信息
        std::cout << "[TaskRTSPStream] 写入流头信息" << std::endl;
        AVDictionary *header_opts = nullptr;
        ret = avformat_write_header(ofmt_ctx_, &header_opts);
        if (header_opts)
            av_dict_free(&header_opts);

        if (ret < 0)
        {
            char errbuf[AV_ERROR_MAX_STRING_SIZE];
            av_strerror(ret, errbuf, AV_ERROR_MAX_STRING_SIZE);
            std::cout << "[TaskRTSPStream] 写入流头信息失败: " << errbuf << std::endl;
            cleanup();
            return false;
        }
        std::cout << "[TaskRTSPStream] 写入流头信息完成" << std::endl;

        std::cout << "[TaskRTSPStream] 打开RTSP推流器成功" << std::endl;
        return true;
    }
    catch (const std::exception &e)
    {
        cleanup();
        return false;
    }
}

// 推送一帧到RTSP流
void FFmpegRtspPusher::pushFrame(const cv::Mat &bgr)
{
    if (!ofmt_ctx_ || !codec_ctx_ || !video_st_ || bgr.empty())
        return;

    // 如果客户端已断开，跳过推流避免崩溃
    if (clientDisconnected_)
        return;

    // 自动调整尺寸
    cv::Mat frame_to_encode = bgr;
    if (bgr.cols != width_ || bgr.rows != height_)
    {
        cv::resize(bgr, frame_to_encode, cv::Size(width_, height_), 0, 0, cv::INTER_LINEAR);
    }

    // 分配AVFrame
    AVFrame *frame = av_frame_alloc();
    if (!frame)
        return;

    frame->format = AV_PIX_FMT_YUV420P;
    frame->width = width_;
    frame->height = height_;
    frame->pts = frameIndex_++;

    if (frameIndex_ % codec_ctx_->gop_size == 1)
    {
        frame->key_frame = 1;
        frame->pict_type = AV_PICTURE_TYPE_I;
    }

    if (av_frame_get_buffer(frame, 32) < 0)
    {
        av_frame_free(&frame);
        return;
    }

    // CUDA BGR到YUV转换
    std::vector<uint8_t> yuv(width_ * height_ * 3 / 2);
    cudaBGR2YUV420P(frame_to_encode, yuv.data(), width_, height_);

    // 设置YUV420P格式的stride
    frame->linesize[0] = width_;
    frame->linesize[1] = width_ / 2;
    frame->linesize[2] = width_ / 2;

    // 复制YUV数据到frame
    int y_size = width_ * height_;
    int uv_size = y_size / 4;

    memcpy(frame->data[0], yuv.data(), y_size);
    memcpy(frame->data[1], yuv.data() + y_size, uv_size);
    memcpy(frame->data[2], yuv.data() + y_size + uv_size, uv_size);

    // 编码并推流，增加错误处理
    int ret = avcodec_send_frame(codec_ctx_, frame);
    if (ret >= 0)
    {
        AVPacket *pkt = av_packet_alloc();
        while (avcodec_receive_packet(codec_ctx_, pkt) >= 0)
        {
            av_packet_rescale_ts(pkt, codec_ctx_->time_base, video_st_->time_base);
            pkt->stream_index = video_st_->index;

            // 写入数据包并检查错误
            int write_ret = av_interleaved_write_frame(ofmt_ctx_, pkt);
            if (write_ret < 0)
            {
                consecutiveErrors_++;
                char errbuf[AV_ERROR_MAX_STRING_SIZE];
                av_strerror(write_ret, errbuf, AV_ERROR_MAX_STRING_SIZE);

                // 检查是否为连接断开错误
                if (write_ret == AVERROR(EPIPE) || write_ret == AVERROR(ECONNRESET) ||
                    write_ret == AVERROR_EOF || consecutiveErrors_ > 5)
                {
                    std::cout << "[FFmpegRtspPusher] 客户端断开连接: " << errbuf
                              << " (连续错误: " << consecutiveErrors_ << ") URL: " << rtspUrl_ << std::endl;
                    clientDisconnected_ = true;
                    av_packet_unref(pkt);
                    av_packet_free(&pkt);
                    av_frame_free(&frame);
                    return;
                }
                else
                {
                    std::cout << "[FFmpegRtspPusher] 写入错误: " << errbuf << std::endl;
                }
            }
            else
            {
                // 写入成功，重置错误计数
                consecutiveErrors_ = 0;
            }
            av_packet_unref(pkt);
        }
        av_packet_free(&pkt);
    }
    else
    {
        char errbuf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, errbuf, AV_ERROR_MAX_STRING_SIZE);
        std::cout << "[FFmpegRtspPusher] 编码错误: " << errbuf << std::endl;
    }
    av_frame_free(&frame);
}

// 关闭RTSP推流器
void FFmpegRtspPusher::close()
{
    if (ofmt_ctx_)
    {
        av_write_trailer(ofmt_ctx_);
        if (!(ofmt_ctx_->oformat->flags & AVFMT_NOFILE))
            avio_closep(&ofmt_ctx_->pb);
    }
    cleanup();
}

// 清理FFmpeg资源
void FFmpegRtspPusher::cleanup()
{
    if (codec_ctx_)
    {
        avcodec_free_context(&codec_ctx_);
        codec_ctx_ = nullptr;
    }

    if (ofmt_ctx_)
    {
        avformat_free_context(ofmt_ctx_);
        ofmt_ctx_ = nullptr;
    }

    if (sws_ctx_)
    {
        sws_freeContext(sws_ctx_);
        sws_ctx_ = nullptr;
    }
    video_st_ = nullptr;
}