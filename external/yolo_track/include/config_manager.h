#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <string>
#include <memory>

// Include nlohmann/json header directly to avoid namespace conflicts
#include <nlohmann/json.hpp>

/**
 * @brief Configuration management class for handling JSON-based configuration
 * Provides centralized configuration management for all system parameters
 */
class ConfigManager
{
public:
    /**
     * @brief Constructor with configuration file path
     * @param config_path Path to the JSON configuration file
     */
    explicit ConfigManager(const std::string &config_path = "config.json");

    /**
     * @brief Destructor
     */
    ~ConfigManager();

    /**
     * @brief Load configuration from file
     * @return true if successful, false otherwise
     */
    bool loadConfig();

    /**
     * @brief Check if configuration is valid
     * @return true if configuration is loaded and valid
     */
    bool isValid() const;

    // Model configuration getters
    std::string getEnginePath() const;
    int getGpuId() const;
    int getNumClass() const;

    // Detection configuration getters
    float getConfidenceThreshold() const;
    float getNmsThreshold() const;

    // Tracking configuration getters
    bool isTrackingEnabled() const;
    int getFrameRate() const;
    int getTrackBuffer() const;
    int getTrackClass() const;
    float getTrackThresh() const;
    float getHighThresh() const;
    float getMatchThresh() const;
    float getUnconfirmedThresh() const;
    float getLowMatchThresh() const;

    // Counting configuration getters
    bool isCountingEnabled() const;
    int getDetectionLineY() const;
    int getMinTargetArea() const;
    bool showLabel() const;

    // Output configuration getters
    bool saveVideo() const;
    bool saveCountingLog() const;
    bool showPerformanceStats() const;

    // Public validation methods
    bool validateTrackingParams() const;
    void printParameterRecommendations() const;

private:
    std::string config_path_;
    nlohmann::json config_;
    bool is_valid_;

    /**
     * @brief Validate configuration parameters
     * @return true if all parameters are valid
     */
    bool validateConfig();

    /**
     * @brief Get value with default fallback
     * @tparam T Type of the value
     * @param path JSON path (e.g., "model.engine_path")
     * @param default_value Default value if path not found
     * @return Value from config or default
     */
    template <typename T>
    T getValue(const std::string &path, const T &default_value) const;
};

#endif // CONFIG_MANAGER_H
