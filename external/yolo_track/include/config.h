#ifndef CONFIG_H
#define CONFIG_H

#include <string>
#include <vector>

const int kGpuId = 0;
const int kNumClass = 1;
const int kInputH = 1280;
const int kInputW = 1280;
const float kNmsThresh = 0.45f;
const float kConfThresh = 0.25f;
const int kMaxNumOutputBbox = 1000; // maximum output detection box number
const int kNumBoxElement = 7;       // left, top, right, bottom, confidence, class, keepflag(whether to keep)


// category names
const std::vector<std::string> vClassNames{
    "iron_plate"};

#endif // CONFIG_H
