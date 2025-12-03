#ifndef INFER_H
#define INFER_H

#include <opencv2/opencv.hpp>
#include "public.h"
#include "config.h"
#include "types.h"

using namespace nvinfer1;

// Forward declaration
class ConfigManager;

class YoloDetector
{
public:
    /**
     * @brief Constructor with configuration manager
     * @param config Configuration manager instance
     */
    explicit YoloDetector(const ConfigManager &config);

    /**
     * @brief Legacy constructor for backward compatibility
     * @param trtFile TensorRT engine file path
     * @param gpuId GPU device ID
     * @param nmsThresh NMS threshold
     * @param confThresh Confidence threshold
     * @param numClass Number of classes
     */
    YoloDetector(const std::string trtFile, int gpuId = kGpuId,
                 float nmsThresh = kNmsThresh, float confThresh = kConfThresh,
                 int numClass = kNumClass);
    ~YoloDetector();
    std::vector<Detection> inference(cv::Mat &img);
    static void draw_image(cv::Mat &img, std::vector<Detection> &inferResult);

private:
    void initialize(); // Common initialization logic
    void get_engine();

private:
    Logger gLogger;
    std::string trtFile_;

    int numClass_;
    float nmsThresh_;
    float confThresh_;
    int gpuId_;

    ICudaEngine *engine;
    IRuntime *runtime;
    IExecutionContext *context;

    cudaStream_t stream;

    float *outputData;
    std::vector<void *> vBufferD;
    float *transposeDevice;
    float *decodeDevice;

    int OUTPUT_CANDIDATES; // 8400: 80 * 80 + 40 * 40 + 20 * 20
};

#endif // INFER_H
