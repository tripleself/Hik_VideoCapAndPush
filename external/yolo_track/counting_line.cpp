#include "counting_line.h"
#include <iostream>
#include <iomanip>

/**
 * @brief ���캯�� - ��ʼ��������ģ��
 * @param frame_width ��Ƶ֡��ȣ����أ������ڻ��Ƽ���ߺͱ߽���
 * @param frame_height ��Ƶ֡�߶ȣ����أ�����������Ĭ�ϼ����λ�úͱ߽���
 * @param fps ��Ƶ֡�ʣ�֡/�룩�����ڼ���֡����ʱ���ʱ���
 * @param video_path ��Ƶ�ļ�·��������������ļ��м�¼Ԫ����
 */
CountingLineModule::CountingLineModule(
    int frame_width,                       // ��Ƶ֡���
    int frame_height,                      // ��Ƶ֡�߶�
    double fps,                            // ��Ƶ֡��
    const std::string &video_path)         // ��Ƶ�ļ�·��
    : frame_width_(frame_width),           // ��Ƶ֡���
      frame_height_(frame_height),         // ��Ƶ֡�߶�
      fps_(fps),                           // ��Ƶ֡��
      frame_duration_ms_(1000.0 / fps),    // ��Ƶ֡����ʱ��
      video_path_(video_path),             // ��Ƶ�ļ�·��
      detection_line_y_(frame_height / 2), // Ĭ�ϼ����λ������Ϊ��Ƶ��������
      show_label_(true),                   // �Ƿ���ʾ����߱�ǩ
      total_count_(0),                     // �ܼ���
      detection_sequence_(0),              // ������к�
      file_initialized_(false)             // �ļ��Ƿ��ʼ��
{
    std::cout << "CountingLineModule initialized: " << frame_width_ << "x" << frame_height_
              << ", " << fps_ << " fps, detection line at y=" << detection_line_y_ << std::endl;
}

/**
 * @brief �������� - ������Դ��ȷ���ļ���ȷ�ر�
 */
CountingLineModule::~CountingLineModule()
{
    if (counting_file_.is_open())
    {
        counting_file_.close();
    }
}

/**
 * @brief ���ü���ߵ�Y����λ��
 * @param y ����ߵ�Y����λ�ã����أ�����ΧӦ��0��frame_height_֮��
 *          y=0��ʾ���涥����y=height-1��ʾ����ײ�
 */
void CountingLineModule::setDetectionLineY(int y)
{
    // ���Y�����Ƿ�����Ч��Χ��
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
 * @brief ���ü����������ļ���·��
 * @param file_path ����ļ�������·���������ļ�������չ��
 *                  �磺"output/counting_results.txt"
 */
void CountingLineModule::setCountingFile(const std::string &file_path)
{
    counting_file_path_ = file_path;
}

/**
 * @brief ��ʼ��������ʼ������ļ���д���ͷ
 * @return bool �ɹ�����true��ʧ�ܷ���false
 *              ʧ��ԭ����ܰ������ļ�·��δ���á��ļ��޷�������
 */
bool CountingLineModule::startCounting()
{
    // ����ļ�·���Ƿ�������
    if (counting_file_path_.empty())
    {
        std::cout << "Error: Counting file path not set" << std::endl;
        return false;
    }

    // ���Դ�����ļ�
    counting_file_.open(counting_file_path_);
    if (!counting_file_.is_open())
    {
        std::cout << "Error: Cannot create counting log file: " << counting_file_path_ << std::endl;
        return false;
    }

    // д��CSV��ʽ�ı�ͷ
    counting_file_ << "Target_ID\treal_processing_time_ms\tCurrent_Frame_Time_ms\tReal_Time_ms\n";
    file_initialized_ = true;

    std::cout << "Counting started, log file: " << counting_file_path_ << std::endl;
    return true;
}

/**
 * @brief ���¼��� - ���Ŀ�괩Խ����߲����м���
 * @param track_results ��ǰ֡�����и��ٽ������������ÿ��Ŀ���λ�á�ID����Ϣ
 * @param current_frame_time_ms ��ǰ֡��ʱ��������룩����ʾ��֡����Ƶ�е�ʱ��λ��
 * @param real_processing_time_ms ����ǰ֡�����ѵ�ʵ��ʱ�䣨���룩���������ܷ���
 * @return int ���ر��θ����������Ĵ�ԽĿ������
 */
int CountingLineModule::updateCounting(const std::vector<TrackResult> &track_results,
                                       double current_frame_time_ms,
                                       double real_processing_time_ms)
{
    int new_crossings = 0; // ��¼���θ��������Ĵ�Խ����

    // ������ǰ֡�е����и���Ŀ��
    for (const auto &track : track_results)
    {
        // �����Ѷ�ʧ��Ŀ�꣨����ʧ�ܵ�Ŀ�꣩
        if (track.is_lost)
            continue;

        int track_id = track.track_id; // ��ȡĿ���Ψһ����ID

        // ����Ŀ��߽������ĵ�����
        // track.bbox��ʽΪ [x1, y1, x2, y2]������(x1,y1)Ϊ���Ͻǣ�(x2,y2)Ϊ���½�
        cv::Point current_center(
            static_cast<int>((track.bbox[0] + track.bbox[2]) / 2), // ���ĵ�X����
            static_cast<int>((track.bbox[1] + track.bbox[3]) / 2)  // ���ĵ�Y����
        );

        // ����Ŀ���Ƿ�����ʷλ�ü�¼������δ������
        // ֻ���������ٵ�Ŀ����ܽ��д�Խ���
        if (previous_positions_.find(track_id) != previous_positions_.end() &&
            counted_targets_.find(track_id) == counted_targets_.end())
        {
            cv::Point prev_center = previous_positions_[track_id]; // ��ȡ��һ֡�����ĵ�λ��

            // ���Ŀ���Ƿ�Խ�˼����
            if (checkLineCrossing(prev_center, current_center))
            {
                // Ŀ�괩Խ����ߣ����м�������
                total_count_++;                    // �����ܼ���
                detection_sequence_++;             // �������кţ�����Target_ID��
                new_crossings++;                   // ���ӱ�����������
                counted_targets_.insert(track_id); // ����Ŀ����Ϊ�Ѽ�������ֹ�ظ�����

                // ������ʵʱ�䣺��ǰ֡ʱ���ȥ����ʱ�䣬�õ�Ŀ��ʵ�ʴ�Խ��ʱ��
                double real_time_ms = current_frame_time_ms - real_processing_time_ms;

                // ����������¼
                CountingRecord record;
                record.sequence_id = detection_sequence_;                 // ����ID����ΪTarget_ID��
                record.real_processing_time_ms = real_processing_time_ms; // ����ʱ��
                record.current_frame_time_ms = current_frame_time_ms;     // ��ǰ֡ʱ��
                record.real_time_ms = real_time_ms;                       // ���������ʵʱ��

                // ����¼�洢���ڴ���
                counting_records_.push_back(record);

                // ����д���ļ�
                writeCountingRecord(record);

                std::cout << "*** Target " << detection_sequence_
                          << " crossed detection line, total count: " << total_count_ << std::endl;
            }
        }

        // ���¸�Ŀ�����ʷλ��Ϊ��ǰλ�ã�����һ֡ʹ��
        previous_positions_[track_id] = current_center;
    }

    return new_crossings;
}

/**
 * @brief ����Ƶ֡�ϻ��Ƽ���ߺͱ�ǩ
 * @param frame �����������Ƶ֡ͼ�񣬺������ڴ�ͼ���ϻ��Ƽ����
 *              ��ҪΪMat���͵Ĳ�ɫͼ��
 */
void CountingLineModule::drawDetectionLine(cv::Mat &frame)
{
    // ����ˮƽ����ߣ���֡�����Ե���ұ�Ե
    // ������ɫ��BGR��ʽ�Ļ�ɫ(0, 255, 255)���߿�3����
    cv::line(frame, cv::Point(0, detection_line_y_), cv::Point(frame_width_, detection_line_y_),
             cv::Scalar(0, 255, 255), 3);

    // �ڼ���߸���������ֱ�ǩ����������˱�ǩ��ʾ��
    if (show_label_)
    {
        cv::putText(frame, "Detection Line", cv::Point(10, detection_line_y_ - 10),
                    cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(0, 255, 255), 2);
    }
}

/**
 * @brief ��ȡ��ǰ�ܼ���
 * @return int �����Կ�ʼ����������Խ����ߵ�Ŀ������
 */
int CountingLineModule::getTotalCount() const
{
    return total_count_;
}

/**
 * @brief ��ȡ��ǰ���к�
 * @return int ���ص�ǰ�ļ�����кţ�Ҳ�������һ��������Ŀ���Target_ID
 */
int CountingLineModule::getCurrentSequence() const
{
    return detection_sequence_;
}

/**
 * @brief ��ɼ�����д��Ԫ���ݲ��ر��ļ�
 * @param total_frames �������֡�����������ļ�ĩβ��¼ͳ����Ϣ
 */
void CountingLineModule::finishCounting(int total_frames)
{
    if (counting_file_.is_open())
    {
        // ���ļ�ĩβд��Ԫ���ݺ�ͳ����Ϣ
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
 * @brief ��ȡ���м�����¼������
 * @return const std::vector<CountingRecord>& ���ش洢���м�����¼�������ĳ�������
 */
const std::vector<CountingRecord> &CountingLineModule::getCountingRecords() const
{
    return counting_records_;
}

/**
 * @brief �����Ƿ���ʾ����߱�ǩ
 * @param show true��ʾ��ʾ��ǩ��false��ʾ���ر�ǩ
 */
void CountingLineModule::setShowLabel(bool show)
{
    show_label_ = show;
}

/**
 * @brief ���ü���ģ�鵽��ʼ״̬
 * ������м������ݡ���ʷλ�á��Ѽ���Ŀ���¼��
 */
void CountingLineModule::reset()
{
    previous_positions_.clear(); // �����ʷλ�ü�¼
    counted_targets_.clear();    // ����Ѽ���Ŀ���¼
    counting_records_.clear();   // ��ռ�����¼
    total_count_ = 0;            // �����ܼ���
    detection_sequence_ = 0;     // �������к�

    // �ر��ļ�
    if (counting_file_.is_open())
    {
        counting_file_.close();
    }
    file_initialized_ = false;

    std::cout << "CountingLineModule reset" << std::endl;
}

/**
 * @brief ���Ŀ���Ƿ�Խ�˼����
 * @param prev_center Ŀ������һ֡�����ĵ�����
 * @param current_center Ŀ���ڵ�ǰ֡�����ĵ�����
 * @return bool ���Ŀ�괩Խ�˼���߷���true�����򷵻�false
 *
 * ��Խ����߼���
 * - ���ϵ��´�Խ����һ֡�ڼ�����Ϸ�����ǰ֡�ڼ�����·�������
 * - ���µ��ϴ�Խ����һ֡�ڼ�����·�����ǰ֡�ڼ�����Ϸ�������
 * - �����߽������Ŀ�����ĵ������ڼ�����ϵ����
 */
bool CountingLineModule::checkLineCrossing(const cv::Point &prev_center, const cv::Point &current_center)
{
    // ����Ƿ�Խ����ߣ�˫���⣺���ϵ��»���µ��ϣ�
    return ((prev_center.y < detection_line_y_ && current_center.y >= detection_line_y_) || // ���ϵ��´�Խ
            (prev_center.y <= detection_line_y_ && current_center.y > detection_line_y_) || // ���ϵ��´�Խ���߽磩
            (prev_center.y > detection_line_y_ && current_center.y <= detection_line_y_) || // ���µ��ϴ�Խ
            (prev_center.y >= detection_line_y_ && current_center.y < detection_line_y_));  // ���µ��ϴ�Խ���߽磩
}

/**
 * @brief ������������¼д���ļ�
 * @param record Ҫд��ļ�����¼����������ID������ʱ�䡢֡ʱ�䡢��ʵʱ�����Ϣ
 *
 * �����ʽ���Ʊ���ָ�����������
 * Target_ID \t real_processing_time_ms \t Current_Frame_Time_ms \t Real_Time_ms
 */
void CountingLineModule::writeCountingRecord(const CountingRecord &record)
{
    if (counting_file_.is_open())
    {
        counting_file_ << record.sequence_id << "\t"                                                   // Target_ID
                       << std::fixed << std::setprecision(2) << record.real_processing_time_ms << "\t" // ����ʱ�䣨����2λС����
                       << std::fixed << std::setprecision(2) << record.current_frame_time_ms << "\t"   // ��ǰ֡ʱ�䣨����2λС����
                       << std::fixed << std::setprecision(2) << record.real_time_ms << "\n";           // ��ʵʱ�䣨����2λС����
        counting_file_.flush();                                                                        // ����ˢ�»�������ȷ������д���ļ�
    }
}