#include "counting_line.h"
#include <iostream>
#include <iomanip>

/**
 * @brief 构造函数 - 初始化计数线模块
 * @param frame_width 视频帧宽度（像素），用于绘制检测线和边界检查
 * @param frame_height 视频帧高度（像素），用于设置默认检测线位置和边界检查
 * @param fps 视频帧率（帧/秒），用于计算帧持续时间和时间戳
 * @param video_path 视频文件路径，用于在输出文件中记录元数据
 */
CountingLineModule::CountingLineModule(
    int frame_width,                       // 视频帧宽度
    int frame_height,                      // 视频帧高度
    double fps,                            // 视频帧率
    const std::string &video_path)         // 视频文件路径
    : frame_width_(frame_width),           // 视频帧宽度
      frame_height_(frame_height),         // 视频帧高度
      fps_(fps),                           // 视频帧率
      frame_duration_ms_(1000.0 / fps),    // 视频帧持续时间
      video_path_(video_path),             // 视频文件路径
      detection_line_y_(frame_height / 2), // 默认检测线位置设置为视频画面中央
      show_label_(true),                   // 是否显示检测线标签
      total_count_(0),                     // 总计数
      detection_sequence_(0),              // 检测序列号
      file_initialized_(false)             // 文件是否初始化
{
    std::cout << "CountingLineModule initialized: " << frame_width_ << "x" << frame_height_
              << ", " << fps_ << " fps, detection line at y=" << detection_line_y_ << std::endl;
}

/**
 * @brief 析构函数 - 清理资源，确保文件正确关闭
 */
CountingLineModule::~CountingLineModule()
{
    if (counting_file_.is_open())
    {
        counting_file_.close();
    }
}

/**
 * @brief 设置检测线的Y坐标位置
 * @param y 检测线的Y坐标位置（像素），范围应在0到frame_height_之间
 *          y=0表示画面顶部，y=height-1表示画面底部
 */
void CountingLineModule::setDetectionLineY(int y)
{
    // 检查Y坐标是否在有效范围内
    if (y >= 0 && y < frame_height_)
    {
        detection_line_y_ = y;
        std::cout << "Detection line position set to y=" << detection_line_y_ << std::endl;
    }
    else
    {
        std::cout << "Warning: Invalid detection line position " << y
                  << ", keeping default y=" << detection_line_y_ << std::endl;
    }
}

/**
 * @brief 设置计数结果输出文件的路径
 * @param file_path 输出文件的完整路径，包含文件名和扩展名
 *                  如："output/counting_results.txt"
 */
void CountingLineModule::setCountingFile(const std::string &file_path)
{
    counting_file_path_ = file_path;
}

/**
 * @brief 开始计数，初始化输出文件并写入表头
 * @return bool 成功返回true，失败返回false
 *              失败原因可能包括：文件路径未设置、文件无法创建等
 */
bool CountingLineModule::startCounting()
{
    // 检查文件路径是否已设置
    if (counting_file_path_.empty())
    {
        std::cout << "Error: Counting file path not set" << std::endl;
        return false;
    }

    // 尝试打开输出文件
    counting_file_.open(counting_file_path_);
    if (!counting_file_.is_open())
    {
        std::cout << "Error: Cannot create counting log file: " << counting_file_path_ << std::endl;
        return false;
    }

    // 写入CSV格式的表头
    counting_file_ << "Target_ID\treal_processing_time_ms\tCurrent_Frame_Time_ms\tReal_Time_ms\n";
    file_initialized_ = true;

    std::cout << "Counting started, log file: " << counting_file_path_ << std::endl;
    return true;
}

/**
 * @brief 更新计数 - 检测目标穿越检测线并进行计数
 * @param track_results 当前帧的所有跟踪结果向量，包含每个目标的位置、ID等信息
 * @param current_frame_time_ms 当前帧的时间戳（毫秒），表示该帧在视频中的时间位置
 * @param real_processing_time_ms 处理当前帧所花费的实际时间（毫秒），用于性能分析
 * @return int 返回本次更新中新增的穿越目标数量
 */
int CountingLineModule::updateCounting(const std::vector<TrackResult> &track_results,
                                       double current_frame_time_ms,
                                       double real_processing_time_ms)
{
    int new_crossings = 0; // 记录本次更新新增的穿越数量

    // 遍历当前帧中的所有跟踪目标
    for (const auto &track : track_results)
    {
        // 跳过已丢失的目标（跟踪失败的目标）
        if (track.is_lost)
            continue;

        int track_id = track.track_id; // 获取目标的唯一跟踪ID

        // 计算目标边界框的中心点坐标
        // track.bbox格式为 [x1, y1, x2, y2]，其中(x1,y1)为左上角，(x2,y2)为右下角
        cv::Point current_center(
            static_cast<int>((track.bbox[0] + track.bbox[2]) / 2), // 中心点X坐标
            static_cast<int>((track.bbox[1] + track.bbox[3]) / 2)  // 中心点Y坐标
        );

        // 检查该目标是否有历史位置记录，且尚未被计数
        // 只有连续跟踪的目标才能进行穿越检测
        if (previous_positions_.find(track_id) != previous_positions_.end() &&
            counted_targets_.find(track_id) == counted_targets_.end())
        {
            cv::Point prev_center = previous_positions_[track_id]; // 获取上一帧的中心点位置

            // 检测目标是否穿越了检测线
            if (checkLineCrossing(prev_center, current_center))
            {
                // 目标穿越检测线，进行计数处理
                total_count_++;                    // 增加总计数
                detection_sequence_++;             // 增加序列号（用作Target_ID）
                new_crossings++;                   // 增加本次新增计数
                counted_targets_.insert(track_id); // 将该目标标记为已计数，防止重复计数

                // 计算真实时间：当前帧时间减去处理时间，得到目标实际穿越的时间
                double real_time_ms = current_frame_time_ms - real_processing_time_ms;

                // 创建计数记录
                CountingRecord record;
                record.sequence_id = detection_sequence_;                 // 序列ID（作为Target_ID）
                record.real_processing_time_ms = real_processing_time_ms; // 处理时间
                record.current_frame_time_ms = current_frame_time_ms;     // 当前帧时间
                record.real_time_ms = real_time_ms;                       // 计算出的真实时间

                // 将记录存储到内存中
                counting_records_.push_back(record);

                // 立即写入文件
                writeCountingRecord(record);

                std::cout << "*** Target " << detection_sequence_
                          << " crossed detection line, total count: " << total_count_ << std::endl;
            }
        }

        // 更新该目标的历史位置为当前位置，供下一帧使用
        previous_positions_[track_id] = current_center;
    }

    return new_crossings;
}

/**
 * @brief 在视频帧上绘制检测线和标签
 * @param frame 输入输出的视频帧图像，函数会在此图像上绘制检测线
 *              需要为Mat类型的彩色图像
 */
void CountingLineModule::drawDetectionLine(cv::Mat &frame)
{
    // 绘制水平检测线，从帧的左边缘到右边缘
    // 线条颜色：BGR格式的黄色(0, 255, 255)，线宽：3像素
    cv::line(frame, cv::Point(0, detection_line_y_), cv::Point(frame_width_, detection_line_y_),
             cv::Scalar(0, 255, 255), 3);

    // 在检测线附近添加文字标签（如果启用了标签显示）
    if (show_label_)
    {
        cv::putText(frame, "Detection Line", cv::Point(10, detection_line_y_ - 10),
                    cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(0, 255, 255), 2);
    }
}

/**
 * @brief 获取当前总计数
 * @return int 返回自开始计数以来穿越检测线的目标总数
 */
int CountingLineModule::getTotalCount() const
{
    return total_count_;
}

/**
 * @brief 获取当前序列号
 * @return int 返回当前的检测序列号，也就是最后一个被计数目标的Target_ID
 */
int CountingLineModule::getCurrentSequence() const
{
    return detection_sequence_;
}

/**
 * @brief 完成计数，写入元数据并关闭文件
 * @param total_frames 处理的总帧数，用于在文件末尾记录统计信息
 */
void CountingLineModule::finishCounting(int total_frames)
{
    if (counting_file_.is_open())
    {
        // 在文件末尾写入元数据和统计信息
        counting_file_ << "# ----------------------------------------\n";
        counting_file_ << "# Total crossings: " << total_count_ << "\n";
        counting_file_ << "# Total frames processed: " << total_frames << "\n";
        counting_file_ << "# Video: " << video_path_ << "\n";
        counting_file_ << "# FPS: " << fps_ << "\n";
        counting_file_ << "# Frame duration: " << frame_duration_ms_ << " ms\n";
        counting_file_.close();

        std::cout << "Counting finished. Results saved to: " << counting_file_path_ << std::endl;
        std::cout << "Total crossings: " << total_count_ << std::endl;
    }
}

/**
 * @brief 获取所有计数记录的引用
 * @return const std::vector<CountingRecord>& 返回存储所有计数记录的向量的常量引用
 */
const std::vector<CountingRecord> &CountingLineModule::getCountingRecords() const
{
    return counting_records_;
}

/**
 * @brief 设置是否显示检测线标签
 * @param show true表示显示标签，false表示隐藏标签
 */
void CountingLineModule::setShowLabel(bool show)
{
    show_label_ = show;
}

/**
 * @brief 重置计数模块到初始状态
 * 清空所有计数数据、历史位置、已计数目标记录等
 */
void CountingLineModule::reset()
{
    previous_positions_.clear(); // 清空历史位置记录
    counted_targets_.clear();    // 清空已计数目标记录
    counting_records_.clear();   // 清空计数记录
    total_count_ = 0;            // 重置总计数
    detection_sequence_ = 0;     // 重置序列号

    // 关闭文件
    if (counting_file_.is_open())
    {
        counting_file_.close();
    }
    file_initialized_ = false;

    std::cout << "CountingLineModule reset" << std::endl;
}

/**
 * @brief 检查目标是否穿越了检测线
 * @param prev_center 目标在上一帧的中心点坐标
 * @param current_center 目标在当前帧的中心点坐标
 * @return bool 如果目标穿越了检测线返回true，否则返回false
 *
 * 穿越检测逻辑：
 * - 从上到下穿越：上一帧在检测线上方，当前帧在检测线下方或线上
 * - 从下到上穿越：上一帧在检测线下方，当前帧在检测线上方或线上
 * - 包含边界情况：目标中心点正好在检测线上的情况
 */
bool CountingLineModule::checkLineCrossing(const cv::Point &prev_center, const cv::Point &current_center)
{
    // 检测是否穿越检测线（双向检测：从上到下或从下到上）
    return ((prev_center.y < detection_line_y_ && current_center.y >= detection_line_y_) || // 从上到下穿越
            (prev_center.y <= detection_line_y_ && current_center.y > detection_line_y_) || // 从上到下穿越（边界）
            (prev_center.y > detection_line_y_ && current_center.y <= detection_line_y_) || // 从下到上穿越
            (prev_center.y >= detection_line_y_ && current_center.y < detection_line_y_));  // 从下到上穿越（边界）
}

/**
 * @brief 将单条计数记录写入文件
 * @param record 要写入的计数记录，包含序列ID、处理时间、帧时间、真实时间等信息
 *
 * 输出格式：制表符分隔的四列数据
 * Target_ID \t real_processing_time_ms \t Current_Frame_Time_ms \t Real_Time_ms
 */
void CountingLineModule::writeCountingRecord(const CountingRecord &record)
{
    if (counting_file_.is_open())
    {
        counting_file_ << record.sequence_id << "\t"                                                   // Target_ID
                       << std::fixed << std::setprecision(2) << record.real_processing_time_ms << "\t" // 处理时间（保留2位小数）
                       << std::fixed << std::setprecision(2) << record.current_frame_time_ms << "\t"   // 当前帧时间（保留2位小数）
                       << std::fixed << std::setprecision(2) << record.real_time_ms << "\n";           // 真实时间（保留2位小数）
        counting_file_.flush();                                                                        // 立即刷新缓冲区，确保数据写入文件
    }
}