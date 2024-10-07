#pragma once

#include "yolov10.h"
#include <opencv2/opencv.hpp>

class AlgYolo10{
public:
    AlgYolo10() {}
    ~AlgYolo10() {}

    int Init(const char *model_path);
    int Deinit();

    int Detect(const cv::Mat& image);

private:
    rknn_app_context_t m_rknn_app_ctx;
}