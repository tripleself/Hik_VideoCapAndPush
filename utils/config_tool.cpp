#include <iostream>
#include <fstream>
#include <string>
#include <nlohmann/json.hpp>
#include "head/ObjectTrackingConfig.h"

using json = nlohmann::json;

/**
 * @brief 配置管理工具
 * 用于检查、验证和修改目标追踪系统的配置参数
 */
class ConfigTool
{
public:
    /**
     * @brief 显示当前配置
     */
    static void showConfig(const std::string &configFile = "config.json")
    {
        try
        {
            std::ifstream file(configFile);
            if (!file.is_open())
            {
                std::cerr << "错误: 无法打开配置文件 " << configFile << std::endl;
                return;
            }

            json config;
            file >> config;
            file.close();

            ObjectTrackingConfig trackingConfig;
            if (trackingConfig.loadFromJson(config))
            {
                std::cout << "\n当前配置已成功加载并验证。" << std::endl;
            }
            else
            {
                std::cout << "\n配置加载失败，请检查配置文件格式。" << std::endl;
            }
        }
        catch (const std::exception &e)
        {
            std::cerr << "配置文件解析错误: " << e.what() << std::endl;
        }
    }

    /**
     * @brief 验证配置文件
     */
    static bool validateConfig(const std::string &configFile = "config.json")
    {
        try
        {
            std::ifstream file(configFile);
            if (!file.is_open())
            {
                std::cerr << "错误: 无法打开配置文件 " << configFile << std::endl;
                return false;
            }

            json config;
            file >> config;
            file.close();

            ObjectTrackingConfig trackingConfig;
            if (!trackingConfig.loadFromJson(config))
            {
                std::cout << "配置加载失败" << std::endl;
                return false;
            }

            if (!trackingConfig.isValid())
            {
                std::cout << "配置参数验证失败" << std::endl;
                return false;
            }

            std::cout << "✓ 配置文件验证通过" << std::endl;
            return true;
        }
        catch (const std::exception &e)
        {
            std::cerr << "配置文件验证错误: " << e.what() << std::endl;
            return false;
        }
    }

    /**
     * @brief 创建示例配置文件
     */
    static void createExampleConfig(const std::string &outputFile = "config_template.json")
    {
        json config = {
            {"camera_count", 1},
            {"rtsp_server", {{"exe_path", "D:\\rtspserver\\rtsp-simple-server.exe"}, {"config_path", "D:\\rtspserver\\rtsp-simple-server.yml"}}},
            {"hikvision_devices", json::array({{{"name", "一位端"},
                                                {"ip", "192.168.0.64"},
                                                {"port", 8553},
                                                {"username", "admin"},
                                                {"password", "tkytjsyjs111"},
                                                {"thermal_channel", 201},
                                                {"visible_channel", 101}}})},
            {"stream_urls", {{"local_ip1", "127.0.0.1"}, {"local_ip2", "127.0.0.1"}, {"rtsp_port", 8556}}},
            {"object_tracking", {{"yolo_detector", {{"model_path", "./models/fil.plan"}, {"gpu_id", 0}, {"confidence_threshold", 0.25}, {"nms_threshold", 0.25}, {"num_classes", 1}}}, {"bytetrack_tracker", {{"frame_rate", 25}, {"track_buffer", 30}, {"use_reid", false}, {"track_class", 0}}}, {"video_processing", {{"video_width", 1920}, {"video_height", 1080}, {"processing_fps", 25}}}, {"counting_module", {{"enable_counting", true}, {"show_detection_line", true}, {"detection_line_offset", 324}, {"counting_output_path", "./counting_results.txt"}}}, {"display", {{"enable_display", true}, {"window_name", "Object Tracking"}, {"window_width", 800}, {"window_height", 600}}}, {"performance", {{"thread_sleep_ms", 10}, {"enable_performance_stats", false}, {"stats_update_interval", 30}}}, {"location_report", {{"enable_location_report", true}, {"udp_server_ip", "192.168.1.115"}, {"udp_server_port", 12345}, {"can_device_type", 2}, {"can_device_index", 0}}}}}};

        try
        {
            std::ofstream file(outputFile);
            file << config.dump(2) << std::endl;
            file.close();
            std::cout << "✓ 示例配置文件已创建: " << outputFile << std::endl;
        }
        catch (const std::exception &e)
        {
            std::cerr << "创建示例配置文件失败: " << e.what() << std::endl;
        }
    }

    /**
     * @brief 显示帮助信息
     */
    static void showHelp()
    {
        std::cout << "\n目标追踪系统配置工具\n";
        std::cout << "===================\n\n";
        std::cout << "用法: config_tool [选项] [配置文件]\n\n";
        std::cout << "选项:\n";
        std::cout << "  -s, --show      显示当前配置\n";
        std::cout << "  -v, --validate  验证配置文件\n";
        std::cout << "  -c, --create    创建示例配置文件\n";
        std::cout << "  -h, --help      显示此帮助信息\n\n";
        std::cout << "示例:\n";
        std::cout << "  config_tool -s                    # 显示 config.json 的配置\n";
        std::cout << "  config_tool -v config.json        # 验证 config.json\n";
        std::cout << "  config_tool -c template.json      # 创建示例配置文件\n\n";
    }
};

int main(int argc, char *argv[])
{
    std::cout << "目标追踪系统配置工具 v1.0\n"
              << std::endl;

    if (argc < 2)
    {
        ConfigTool::showHelp();
        return 0;
    }

    std::string option = argv[1];
    std::string configFile = (argc > 2) ? argv[2] : "config.json";

    if (option == "-s" || option == "--show")
    {
        std::cout << "显示配置文件: " << configFile << "\n"
                  << std::endl;
        ConfigTool::showConfig(configFile);
    }
    else if (option == "-v" || option == "--validate")
    {
        std::cout << "验证配置文件: " << configFile << "\n"
                  << std::endl;
        bool isValid = ConfigTool::validateConfig(configFile);
        return isValid ? 0 : 1;
    }
    else if (option == "-c" || option == "--create")
    {
        std::cout << "创建示例配置文件: " << configFile << "\n"
                  << std::endl;
        ConfigTool::createExampleConfig(configFile);
    }
    else if (option == "-h" || option == "--help")
    {
        ConfigTool::showHelp();
    }
    else
    {
        std::cerr << "未知选项: " << option << std::endl;
        ConfigTool::showHelp();
        return 1;
    }

    return 0;
}