#include <opencv2/opencv.hpp>
#include <iostream>
#include <vector>

int main()
{
    std::cout << "=== OpenCV Build Information ===" << std::endl;
    std::cout << cv::getBuildInformation() << std::endl;

    std::cout << "\n=== Testing Video Codecs ===" << std::endl;

    // Test different codecs
    std::vector<std::pair<std::string, int>> codecs = {
        {"MJPEG", cv::VideoWriter::fourcc('M', 'J', 'P', 'G')},
        {"H264", cv::VideoWriter::fourcc('H', '2', '6', '4')},
        {"X264", cv::VideoWriter::fourcc('X', '2', '6', '4')},
        {"XVID", cv::VideoWriter::fourcc('X', 'V', 'I', 'D')},
        {"MP4V", cv::VideoWriter::fourcc('M', 'P', '4', 'V')},
        {"WMV2", cv::VideoWriter::fourcc('W', 'M', 'V', '2')},
        {"DIVX", cv::VideoWriter::fourcc('D', 'I', 'V', 'X')},
        {"Uncompressed", 0}};

    std::string testFile = "test_codec.avi";
    cv::Size frameSize(640, 480);
    double fps = 25.0;

    for (const auto &codec : codecs)
    {
        std::cout << "\nTesting " << codec.first << " (fourcc: " << codec.second << ")..." << std::endl;

        try
        {
            cv::VideoWriter writer(testFile, codec.second, fps, frameSize, true);

            if (writer.isOpened())
            {
                std::cout << "✅ " << codec.first << " - SUPPORTED" << std::endl;
                writer.release();

                // Create a test frame and try to write
                cv::Mat testFrame = cv::Mat::zeros(frameSize, CV_8UC3);
                cv::VideoWriter testWriter(testFile, codec.second, fps, frameSize, true);
                if (testWriter.isOpened())
                {
                    testWriter.write(testFrame);
                    testWriter.release();
                    std::cout << "   Write test: SUCCESS" << std::endl;
                }
                else
                {
                    std::cout << "   Write test: FAILED" << std::endl;
                }
            }
            else
            {
                std::cout << "❌ " << codec.first << " - NOT SUPPORTED" << std::endl;
            }
        }
        catch (const cv::Exception &e)
        {
            std::cout << "❌ " << codec.first << " - EXCEPTION: " << e.what() << std::endl;
        }

        // Clean up test file
        std::remove(testFile.c_str());
    }

    std::cout << "\n=== Backend Information ===" << std::endl;
    std::vector<cv::VideoCaptureAPIs> backends = cv::videoio_registry::getBackends();
    std::cout << "Available VideoIO backends:" << std::endl;
    for (auto backend : backends)
    {
        std::cout << "  - " << cv::videoio_registry::getBackendName(backend) << std::endl;
    }

    return 0;
}
