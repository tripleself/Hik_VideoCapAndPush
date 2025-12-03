#include "TaskLocationReporter.h"
#include <iostream>
#include <chrono>

/**
 * @brief Constructor, initialize location reporting task
 * @param data Shared data reference
 * @param config Object tracking configuration containing location report settings
 */
TaskLocationReporter::TaskLocationReporter(SharedData &data, const ObjectTrackingConfig &config)
    : data_(data), config_(config)
{
    std::cout << "[TaskLocationReporter] Initialize location reporting task, TCP port: " << config_.tcpServerPort
              << ", check interval: " << config_.checkIntervalMs << "ms" << std::endl;

    // Temporary test code
}

/**
 * @brief Destructor, ensure thread safe stop
 */
TaskLocationReporter::~TaskLocationReporter()
{
    std::cout << "[TaskLocationReporter] Starting destructor..." << std::endl;
    stop(); // Ensure thread safe stop
    std::cout << "[TaskLocationReporter] Destructor completed" << std::endl;
}

/**
 * @brief Start location reporting thread
 */
void TaskLocationReporter::start()
{
    std::cout << "[TaskLocationReporter] Starting location reporting thread..." << std::endl;

    try
    {
        // 1. Create LocationReporter instance (reuse existing code)
        locationReporter_ = std::make_unique<LocationReporter>(config_.tcpServerPort, &config_);

        // 2. Initialize LocationReporter (includes CAN device and TCP server)
        if (!locationReporter_->initialize())
        {
            std::cerr << "[TaskLocationReporter] LocationReporter initialization failed" << std::endl;
            locationReporter_.reset();
            return;
        }

        // 3. Start reporting thread
        isRunning_ = true;
        thread_ = std::thread(&TaskLocationReporter::run, this);

        std::cout << "[TaskLocationReporter] Location reporting thread started successfully" << std::endl;
    }
    catch (const std::exception &e)
    {
        std::cerr << "[TaskLocationReporter] Start failed: " << e.what() << std::endl;
        isRunning_ = false;
    }
}

/**
 * @brief Stop location reporting thread
 */
void TaskLocationReporter::stop()
{
    std::cout << "[TaskLocationReporter] Stopping location reporting thread..." << std::endl;

    // 1. Set stop flag
    isRunning_ = false;

    // 2. Wait for thread to finish
    if (thread_.joinable())
    {
        thread_.join();
    }

    // 3. Clean up LocationReporter resources
    // Note: LocationReporter destructor will handle cleanup automatically
    if (locationReporter_)
    {
        locationReporter_.reset();
    }

    std::cout << "[TaskLocationReporter] Location reporting thread stopped" << std::endl;
}

/**
 * @brief Check if reporting service is ready
 * @return true if TCP server is started and has client connections
 */
bool TaskLocationReporter::isReady() const
{
    return locationReporter_ && locationReporter_->isReady();
}

/**
 * @brief Get number of connected clients
 * @return Client count
 */
size_t TaskLocationReporter::getClientCount() const
{
    return locationReporter_ ? locationReporter_->getClientCount() : 0;
}

/**
 * @brief Thread main function, periodically check flags and execute reporting
 *
 * Core logic:
 * 1. Periodically check 4 detection flags in SharedData
 * 2. Use exchange() atomic operation to read and reset flags
 * 3. Call LocationReporter for unified reporting
 * 4. Control checking frequency to avoid excessive CPU usage
 */
void TaskLocationReporter::run()
{
    std::cout << "[TaskLocationReporter] Reporting thread started..." << std::endl;
    while (isRunning_ && data_.isRunning)
    {
        try
        {
            // 1. Atomic operation to read and reset detection flags
            // Use exchange() to ensure thread safety while resetting flags to avoid duplicate reporting
            uint8_t camera1_visible = data_.camera1_visible_detected.exchange(false) ? 1 : 0;
            uint8_t camera1_thermal = data_.camera1_thermal_detected.exchange(false) ? 1 : 0;
            uint8_t camera2_visible = data_.camera2_visible_detected.exchange(false) ? 1 : 0;
            uint8_t camera2_thermal = data_.camera2_thermal_detected.exchange(false) ? 1 : 0;

            // 2. Call LocationReporter for unified reporting
            // Note: LocationReporter internally handles CAN data reading, packet assembly, network transmission etc.
            if (locationReporter_)
            {
                locationReporter_->reportLocation(camera1_visible, camera1_thermal,
                                                  camera2_visible, camera2_thermal);
            }

            // 3. Output debug info (only when targets are detected)
            if (camera1_visible || camera1_thermal || camera2_visible || camera2_thermal)
            {
                std::cout << "[TaskLocationReporter] Detection status report: "
                          << "camera1_visible=" << (int)camera1_visible
                          << ", camera1_thermal=" << (int)camera1_thermal
                          << ", camera2_visible=" << (int)camera2_visible
                          << ", camera2_thermal=" << (int)camera2_thermal << std::endl;
            }

            // 4. Control checking frequency to avoid excessive CPU usage
            std::this_thread::sleep_for(std::chrono::milliseconds(config_.checkIntervalMs));
        }
        catch (const std::exception &e)
        {
            std::cerr << "[TaskLocationReporter] Reporting thread exception: " << e.what() << std::endl;
            // Wait briefly on exception to avoid exception loops
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
    }

    std::cout << "[TaskLocationReporter] Reporting thread exiting" << std::endl;
}