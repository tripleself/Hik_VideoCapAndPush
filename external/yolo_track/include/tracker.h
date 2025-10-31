#ifndef TRACKER_H
#define TRACKER_H

#include <opencv2/opencv.hpp>
#include <vector>
#include "types.h"
#include "BYTETracker.h"

// Forward declaration
class ConfigManager;

// 追踪结果结构
struct TrackResult
{
    int track_id;
    float bbox[4]; // x1, y1, x2, y2
    float conf;    // confidence
    int classId;   // class ID
    bool is_new;   // whether is new target
    bool is_lost;  // whether is lost
};

// tracking module class
class TrackerModule
{
public:
    /**
     * @brief Constructor with configuration manager
     * @param config Configuration manager instance
     */
    explicit TrackerModule(const ConfigManager &config);

    /**
     * @brief Legacy constructor for backward compatibility
     * @param frame_rate Video frame rate
     * @param track_buffer Track buffer size
     * @param track_class Target class to track
     */
    TrackerModule(int frame_rate = 30, int track_buffer = 30, int track_class = 0);
    ~TrackerModule();

    // update tracking, input detection results, return tracking results
    std::vector<TrackResult> update(const std::vector<Detection> &detections);

    // set tracking class
    void setTrackClass(int class_id);

    // get tracking statistics
    int getActiveTrackCount() const;
    int getTotalTrackCount() const;

    // draw tracking results
    static void drawTrackResults(cv::Mat &img, const std::vector<TrackResult> &track_results);

    // get class color
    static cv::Scalar getClassColor(int class_id);

private:
    // check if it is the class to be tracked
    bool isTrackingClass(int class_id) const;

    // convert detection results to ByteTrack input format
    std::vector<Object> detectionsToObjects(const std::vector<Detection> &detections);

    // convert ByteTrack output to tracking results
    std::vector<TrackResult> stracksToTrackResults(const std::vector<STrack> &stracks,
                                                   const std::vector<Detection> &original_detections);

private:
    BYTETracker *byte_tracker_; // ByteTrack tracker
    int track_class_;           // tracking class
    int total_track_count_;     // total tracking count
    int min_target_area_;       // minimum target area for filtering

    // store detection results to keep class information when tracking
    std::vector<Detection> current_detections_;
};

#endif // TRACKER_H