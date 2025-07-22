#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <opencv2/opencv.hpp>  // OpenCV C++ headers

extern "C" {
#include "image_utils.h"
}

// Normalize to [0,1] or use mean/std if desired
static inline float normalize_pixel(uint8_t value) {
    return value / 255.0f;
}

int load_image_as_tensor(const char* image_path, const int64_t* input_shape, float** output) {
    const int channels = (int)input_shape[1];  // typically 3
    const int height   = (int)input_shape[2];
    const int width    = (int)input_shape[3];

    // Load image using OpenCV
    cv::Mat img = cv::imread(image_path, cv::IMREAD_COLOR);
    if (img.empty()) {
        fprintf(stderr, "[ERROR] Could not load image: %s\n", image_path);
        return 1;
    }

    // Resize
    cv::resize(img, img, cv::Size(width, height));
    img.convertTo(img, CV_8UC3);  // Ensure format

    // Allocate CHW buffer
    size_t tensor_size = channels * height * width;
    float* buffer = (float*)malloc(sizeof(float) * tensor_size);
    if (!buffer) {
        fprintf(stderr, "[ERROR] Could not allocate image tensor buffer.\n");
        return 1;
    }

    // Convert BGR HWC to CHW format with normalization
    for (int c = 0; c < channels; ++c) {
        for (int h = 0; h < height; ++h) {
            for (int w = 0; w < width; ++w) {
                uint8_t pixel = img.at<cv::Vec3b>(h, w)[2 - c];  // OpenCV = BGR → we reorder to RGB
                buffer[c * height * width + h * width + w] = normalize_pixel(pixel);
            }
        }
    }

    *output = buffer;
    return 0;
}
