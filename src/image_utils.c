#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "image_utils.h"

// Normalize pixel value to [0,1] range
static inline float normalize_pixel(uint8_t value) {
    return value / 255.0f;
}

// Load raw RGB data from file (expects width*height*3 bytes in RGB format)
static int load_raw_rgb(const char* image_path, int width, int height, uint8_t** rgb_data) {
    FILE* file = fopen(image_path, "rb");
    if (!file) {
        fprintf(stderr, "[ERROR] Could not open file: %s\n", image_path);
        return 1;
    }

    size_t expected_size = width * height * 3;
    *rgb_data = (uint8_t*)malloc(expected_size);
    if (!*rgb_data) {
        fprintf(stderr, "[ERROR] Could not allocate memory for image data.\n");
        fclose(file);
        return 1;
    }

    size_t bytes_read = fread(*rgb_data, 1, expected_size, file);
    fclose(file);

    if (bytes_read != expected_size) {
        fprintf(stderr, "[ERROR] Expected %zu bytes, but read %zu bytes from %s\n", 
                expected_size, bytes_read, image_path);
        free(*rgb_data);
        *rgb_data = NULL;
        return 1;
    }

    return 0;
}

// Create synthetic test data (gradient pattern)
static int create_test_data(int width, int height, uint8_t** rgb_data) {
    size_t data_size = width * height * 3;
    *rgb_data = (uint8_t*)malloc(data_size);
    if (!*rgb_data) {
        fprintf(stderr, "[ERROR] Could not allocate memory for test image data.\n");
        return 1;
    }

    // Create a simple gradient pattern
    for (int h = 0; h < height; h++) {
        for (int w = 0; w < width; w++) {
            int idx = (h * width + w) * 3;
            (*rgb_data)[idx + 0] = (uint8_t)((w * 255) / width);      // Red gradient
            (*rgb_data)[idx + 1] = (uint8_t)((h * 255) / height);     // Green gradient  
            (*rgb_data)[idx + 2] = (uint8_t)(((w + h) * 255) / (width + height)); // Blue gradient
        }
    }

    printf("[INFO] Created synthetic test image (%dx%d)\n", width, height);
    return 0;
}

int load_image_as_tensor(const char* image_path, const int64_t* input_shape, float** output) {
    const int channels = (int)input_shape[1];  // typically 3
    const int height   = (int)input_shape[2];
    const int width    = (int)input_shape[3];

    if (channels != 3) {
        fprintf(stderr, "[ERROR] Only 3-channel images are supported (got %d channels)\n", channels);
        return 1;
    }

    uint8_t* rgb_data = NULL;
    int load_result = -1;

    // Try to load as raw RGB file first, if that fails, create test data
    if (image_path && strlen(image_path) > 0) {
        load_result = load_raw_rgb(image_path, width, height, &rgb_data);
        if (load_result != 0) {
            printf("[WARN] Could not load image from %s, creating synthetic test data instead\n", image_path);
        }
    }

    // If loading failed or no path provided, create test data
    if (load_result != 0) {
        load_result = create_test_data(width, height, &rgb_data);
        if (load_result != 0) {
            return 1;
        }
    }

    // Allocate CHW buffer for the tensor
    size_t tensor_size = channels * height * width;
    float* buffer = (float*)malloc(sizeof(float) * tensor_size);
    if (!buffer) {
        fprintf(stderr, "[ERROR] Could not allocate image tensor buffer.\n");
        free(rgb_data);
        return 1;
    }

    // Convert RGB HWC to CHW format with normalization
    for (int c = 0; c < channels; ++c) {
        for (int h = 0; h < height; ++h) {
            for (int w = 0; w < width; ++w) {
                int src_idx = (h * width + w) * 3 + c;  // RGB HWC format
                int dst_idx = c * height * width + h * width + w;  // CHW format
                buffer[dst_idx] = normalize_pixel(rgb_data[src_idx]);
            }
        }
    }

    free(rgb_data);
    *output = buffer;
    return 0;
}
