#pragma once
#include <opencv2/opencv.hpp>

// CUDA function for BGR to YUV420P conversion
void cudaBGR2YUV420P(const cv::Mat &bgr, uint8_t *yuv, int width, int height);