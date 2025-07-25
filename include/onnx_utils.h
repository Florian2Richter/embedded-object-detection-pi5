#ifndef ONNX_UTILS_H
#define ONNX_UTILS_H

#include <onnxruntime_c_api.h>
#include <stdint.h>

// Structure to hold detection results
typedef struct {
    float x1, y1, x2, y2;  // Bounding box coordinates
    float confidence;      // Detection confidence
    int class_id;         // Class ID
} Detection;

// Structure to hold all detections
typedef struct {
    Detection* detections;
    int count;
    int capacity;
} DetectionResults;

int init_env(const OrtApi* ort, OrtEnv** env);
int load_model(const OrtApi* ort, OrtEnv* env, const char* model_path, OrtSession** session);
int run_inference(const OrtApi* ort, OrtSession* session, float* input_data, int64_t* input_shape);

// Helper functions for data type conversion
uint16_t* convert_float32_to_float16(const float* input, size_t count);
float float16_to_float32(uint16_t f16_bits);

// Detection postprocessing functions
DetectionResults* create_detection_results(int initial_capacity);
void free_detection_results(DetectionResults* results);
int add_detection(DetectionResults* results, float x1, float y1, float x2, float y2, float confidence, int class_id);

// Parse model output into detections
int parse_model_output(const OrtApi* ort, OrtValue* output_tensor, float confidence_threshold, DetectionResults** results);

// Display detection results
void print_detection_results(const DetectionResults* results);

#endif  // ONNX_UTILS_H
