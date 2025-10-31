#pragma once

#include "SharedData.h"
#include "LocationReporter.h"
#include "ObjectTrackingConfig.h"
#include <thread>
#include <atomic>
#include <memory>

/**
 * @brief Location reporting task class
 *
 * Responsibilities:
 * 1. Run independent thread, periodically check 4 detection flags in SharedData
 * 2. Unified management of all location reporting logic
 * 3. Maintain consistent start/stop interface with other task classes
 *
 * Design principles:
 * - Detection tasks only set flags
 * - Reporting task only reads flags and reports
 * - Complete separation of detection and reporting
 */
class TaskLocationReporter
{
public:
    /**
     * @brief Constructor
     * @param data Shared data reference
     * @param config Object tracking configuration containing location report settings
     */
    TaskLocationReporter(SharedData &data, const ObjectTrackingConfig &config);

    /**
     * @brief Destructor, ensure thread safe stop
     */
    ~TaskLocationReporter();

    /**
     * @brief Start location reporting thread
     */
    void start();

    /**
     * @brief Stop location reporting thread
     */
    void stop();

    /**
     * @brief Check if reporting service is ready
     * @return true if TCP server is started and has client connections
     */
    bool isReady() const;

    /**
     * @brief Get number of connected clients
     * @return Client count
     */
    size_t getClientCount() const;

private:
    /**
     * @brief Thread main function, periodically check flags and execute reporting
     */
    void run();

    // Core member variables
    SharedData &data_;                                   // Shared data reference
    std::unique_ptr<LocationReporter> locationReporter_; // Location reporter instance
    std::thread thread_;                                 // Reporting thread
    std::atomic<bool> isRunning_{false};                 // Thread running state

    // Configuration parameters
    const ObjectTrackingConfig &config_; // Configuration reference
};