#ifndef COUNTING_LINE_H
#define COUNTING_LINE_H

#include <opencv2/opencv.hpp>
#include <fstream>
#include <map>
#include <set>
#include <string>
#include <vector>
#include "tracker.h"

// Forward declaration
class ConfigManager;

// counting record structure
struct CountingRecord
{
    int sequence_id;
    double real_processing_time_ms;
    double current_frame_time_ms;
    double real_time_ms;
};

// virtual detection line counting module
class CountingLineModule
{
public:
    /**
     * @brief Constructor with configuration manager
     * @param frame_width Video frame width
     * @param frame_height Video frame height
     * @param fps Video frame rate
     * @param config Configuration manager instance
     * @param video_path Video file path (optional)
     */
    CountingLineModule(int frame_width, int frame_height, double fps,
                       const ConfigManager &config, const std::string &video_path = "");

    /**
     * @brief Legacy constructor for backward compatibility
     * @param frame_width Video frame width
     * @param frame_height Video frame height
     * @param fps Video frame rate
     * @param video_path Video file path (optional)
     */
    CountingLineModule(int frame_width, int frame_height, double fps, const std::string &video_path = "");

    // Destructor
    ~CountingLineModule();

    // set detection line position (default to video center)
    void setDetectionLineY(int y);

    // set counting record file path
    void setCountingFile(const std::string &file_path);

    // start counting (create record file)
    bool startCounting();

    // update counting (input tracking results, return new count)
    int updateCounting(const std::vector<TrackResult> &track_results, double current_frame_time_ms, double real_processing_time_ms);

    // draw detection line to frame
    void drawDetectionLine(cv::Mat &frame);

    // get total count
    int getTotalCount() const;

    // get current sequence
    int getCurrentSequence() const;

    // finish counting (write statistics and close file)
    void finishCounting(int total_frames);

    // get all counting records
    const std::vector<CountingRecord> &getCountingRecords() const;

    // set detection line label display
    void setShowLabel(bool show);

    // reset counting
    void reset();

private:
    // check if target crosses detection line
    bool checkLineCrossing(const cv::Point &prev_center, const cv::Point &current_center);

    // write counting record to file
    void writeCountingRecord(const CountingRecord &record);

private:
    // video parameters
    int frame_width_;
    int frame_height_;
    double fps_;
    double frame_duration_ms_;
    std::string video_path_;

    // detection line parameters
    int detection_line_y_;
    bool show_label_;

    // counting status
    std::map<int, cv::Point> previous_positions_; // track_id -> previous frame center point
    std::set<int> counted_targets_;               // counted target ID
    int total_count_;
    int detection_sequence_;

    // record storage
    std::vector<CountingRecord> counting_records_;

    // file operation
    std::string counting_file_path_;
    std::ofstream counting_file_;
    bool file_initialized_;
};

#endif // COUNTING_LINE_H