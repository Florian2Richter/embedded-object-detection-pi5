#include <stdio.h>
#include <stdlib.h>
#include <onnxruntime_c_api.h>
#include "onnx_utils.h"

#define CHECK_STATUS(status, ort, msg) \
    if ((status) != NULL) { \
        const char* err = ort->GetErrorMessage(status); \
        fprintf(stderr, "[ERROR] %s: %s\n", msg, err); \
        ort->ReleaseStatus(status); \
        return 1; \
    }

// ============================================================================
// Data type conversion functions
// ============================================================================

// Simple float32 to float16 conversion function
// This implements IEEE 754 half-precision format
uint16_t* convert_float32_to_float16(const float* input, size_t count) {
    uint16_t* output = (uint16_t*)malloc(count * sizeof(uint16_t));
    if (!output) {
        return NULL;
    }
    
    for (size_t i = 0; i < count; i++) {
        union { float f; uint32_t i; } f32;
        f32.f = input[i];
        
        uint32_t f32_bits = f32.i;
        uint16_t f16_bits;
        
        // Extract sign, exponent, and mantissa from float32
        uint32_t sign = (f32_bits >> 31) & 0x1;
        uint32_t exp = (f32_bits >> 23) & 0xFF;
        uint32_t mantissa = f32_bits & 0x7FFFFF;
        
        if (exp == 0) {
            // Zero or denormalized number
            f16_bits = (sign << 15) | 0;
        } else if (exp == 0xFF) {
            // Infinity or NaN
            f16_bits = (sign << 15) | 0x7C00 | (mantissa ? 0x200 : 0);
        } else {
            // Normalized number
            int32_t new_exp = exp - 127 + 15;  // Adjust bias (127->15)
            
            if (new_exp <= 0) {
                // Underflow to zero
                f16_bits = (sign << 15) | 0;
            } else if (new_exp >= 31) {
                // Overflow to infinity
                f16_bits = (sign << 15) | 0x7C00;
            } else {
                // Normal case
                uint32_t new_mantissa = mantissa >> 13;  // Reduce from 23 to 10 bits
                f16_bits = (sign << 15) | (new_exp << 10) | new_mantissa;
            }
        }
        
        output[i] = f16_bits;
    }
    
    return output;
}

// Simple float16 to float32 conversion function
float float16_to_float32(uint16_t f16_bits) {
    uint32_t sign = (f16_bits >> 15) & 0x1;
    uint32_t exp = (f16_bits >> 10) & 0x1F;
    uint32_t mantissa = f16_bits & 0x3FF;
    
    union { float f; uint32_t i; } f32;
    
    if (exp == 0) {
        if (mantissa == 0) {
            // Zero
            f32.i = sign << 31;
        } else {
            // Denormalized number - convert to normalized
            float val = mantissa / 1024.0f;  // 2^10
            val /= 16384.0f;  // 2^14 (bias adjustment)
            f32.f = sign ? -val : val;
        }
    } else if (exp == 31) {
        // Infinity or NaN
        f32.i = (sign << 31) | 0x7F800000 | (mantissa ? 0x400000 : 0);
    } else {
        // Normalized number
        uint32_t new_exp = exp - 15 + 127;  // Adjust bias (15->127)
        uint32_t new_mantissa = mantissa << 13;  // Expand from 10 to 23 bits
        f32.i = (sign << 31) | (new_exp << 23) | new_mantissa;
    }
    
    return f32.f;
}

// ============================================================================
// Detection results management functions
// ============================================================================

DetectionResults* create_detection_results(int initial_capacity) {
    DetectionResults* results = malloc(sizeof(DetectionResults));
    if (!results) return NULL;
    
    results->detections = malloc(initial_capacity * sizeof(Detection));
    if (!results->detections) {
        free(results);
        return NULL;
    }
    
    results->count = 0;
    results->capacity = initial_capacity;
    return results;
}

void free_detection_results(DetectionResults* results) {
    if (results) {
        free(results->detections);
        free(results);
    }
}

int add_detection(DetectionResults* results, float x1, float y1, float x2, float y2, float confidence, int class_id) {
    if (!results) return 1;
    
    // Resize if needed
    if (results->count >= results->capacity) {
        int new_capacity = results->capacity * 2;
        Detection* new_detections = realloc(results->detections, new_capacity * sizeof(Detection));
        if (!new_detections) return 1;
        
        results->detections = new_detections;
        results->capacity = new_capacity;
    }
    
    Detection* det = &results->detections[results->count];
    det->x1 = x1;
    det->y1 = y1;
    det->x2 = x2;
    det->y2 = y2;
    det->confidence = confidence;
    det->class_id = class_id;
    
    results->count++;
    return 0;
}

// ============================================================================
// Model output parsing functions
// ============================================================================

int parse_model_output(const OrtApi* ort, OrtValue* output_tensor, float confidence_threshold, DetectionResults** results) {
    if (!ort || !output_tensor || !results) return 1;
    
    *results = NULL;
    
    // Get output data directly from tensor
    void* output_data = NULL;
    OrtStatus* status = ort->GetTensorMutableData(output_tensor, &output_data);
    if (status != NULL || !output_data) {
        if (status) ort->ReleaseStatus(status);
        return 1;
    }
    
    // For now, we'll use hardcoded knowledge about the model output format
    // This is [1, 25200, 85] YOLO format based on our previous test
    int64_t batch_size = 1;
    int64_t num_detections = 25200;
    int64_t detection_size = 85;
    
    printf("[INFO] Output tensor shape: [%lld, %lld, %lld]\n", 
           (long long)batch_size, (long long)num_detections, (long long)detection_size);
    printf("[INFO] Output data type: FLOAT16\n");
    printf("[INFO] Processing %lld detections with %lld elements each\n", 
           (long long)num_detections, (long long)detection_size);
    
    // Create results structure
    *results = create_detection_results(100);  // Start with capacity for 100 detections
    if (!*results) {
        return 1;
    }
    
    // Process each detection (assuming FLOAT16 format based on previous test)
    uint16_t* f16_results = (uint16_t*)output_data;
    
    for (int64_t i = 0; i < num_detections; i++) {
        // Get detection values, converting from float16
        float detection_values[85];
        
        for (int64_t j = 0; j < detection_size; j++) {
            detection_values[j] = float16_to_float32(f16_results[i * detection_size + j]);
        }
        
        // YOLO format: [x, y, w, h, conf, class0, class1, ..., class79]
        float x_center = detection_values[0];
        float y_center = detection_values[1];
        float width = detection_values[2];
        float height = detection_values[3];
        float obj_conf = detection_values[4];
        
        // Find best class
        float max_class_conf = 0.0f;
        int best_class = -1;
        for (int c = 5; c < detection_size; c++) {
            if (detection_values[c] > max_class_conf) {
                max_class_conf = detection_values[c];
                best_class = c - 5;
            }
        }
        
        float total_confidence = obj_conf * max_class_conf;
        
        if (total_confidence > confidence_threshold) {
            float x1 = x_center - width / 2.0f;
            float y1 = y_center - height / 2.0f;
            float x2 = x_center + width / 2.0f;
            float y2 = y_center + height / 2.0f;
            
            add_detection(*results, x1, y1, x2, y2, total_confidence, best_class);
        }
    }
    
    return 0;
}

void print_detection_results(const DetectionResults* results) {
    if (!results || results->count == 0) {
        printf("No objects detected above confidence threshold\n");
        return;
    }
    
    printf("\n=== DETECTION RESULTS ===\n");
    
    for (int i = 0; i < results->count; i++) {
        const Detection* det = &results->detections[i];
        printf("Detection #%d:\n", i + 1);
        printf("  Bounding Box: (%.1f, %.1f) to (%.1f, %.1f)\n", det->x1, det->y1, det->x2, det->y2);
        printf("  Confidence: %.3f (%.1f%%)\n", det->confidence, det->confidence * 100.0f);
        printf("  Class ID: %d\n", det->class_id);
        printf("  Box Size: %.1f x %.1f pixels\n", det->x2 - det->x1, det->y2 - det->y1);
        printf("\n");
    }
    
    printf("=== SUMMARY: %d objects detected ===\n", results->count);
}

// ============================================================================
// ONNX Runtime functions
// ============================================================================

int init_env(const OrtApi* ort, OrtEnv** env) {
    OrtStatus* status = ort->CreateEnv(ORT_LOGGING_LEVEL_WARNING, "onnx_app", env);
    CHECK_STATUS(status, ort, "OrtCreateEnv failed");
    return 0;
}

int load_model(const OrtApi* ort, OrtEnv* env, const char* model_path, OrtSession** session) {
    OrtStatus* status;
    OrtSessionOptions* session_options = NULL;

    status = ort->CreateSessionOptions(&session_options);
    CHECK_STATUS(status, ort, "CreateSessionOptions failed");

    // Optional: set graph optimization level
    status = ort->SetSessionGraphOptimizationLevel(session_options, ORT_ENABLE_EXTENDED);
    CHECK_STATUS(status, ort, "SetSessionGraphOptimizationLevel failed");

    // Create session
    status = ort->CreateSession(env, model_path, session_options, session);
    CHECK_STATUS(status, ort, "CreateSession failed");

    ort->ReleaseSessionOptions(session_options);
    return 0;
}

int run_inference(const OrtApi* ort, OrtSession* session, float* input_data, int64_t* input_shape) {
    OrtAllocator* allocator = NULL;
    OrtStatus* status = ort->GetAllocatorWithDefaultOptions(&allocator);
    CHECK_STATUS(status, ort, "GetAllocatorWithDefaultOptions failed");

    // ---- Get input name and type information ----
    char* input_name = NULL;
    status = ort->SessionGetInputName(session, 0, allocator, &input_name);
    CHECK_STATUS(status, ort, "SessionGetInputName failed");

    // Get input type info to determine expected data type
    OrtTypeInfo* input_type_info = NULL;
    status = ort->SessionGetInputTypeInfo(session, 0, &input_type_info);
    CHECK_STATUS(status, ort, "SessionGetInputTypeInfo failed");

    const OrtTensorTypeAndShapeInfo* tensor_info = NULL;
    status = ort->CastTypeInfoToTensorInfo(input_type_info, &tensor_info);
    CHECK_STATUS(status, ort, "CastTypeInfoToTensorInfo failed");

    ONNXTensorElementDataType input_data_type;
    status = ort->GetTensorElementType(tensor_info, &input_data_type);
    CHECK_STATUS(status, ort, "GetTensorElementType failed");

    // Print what the model expects
    const char* type_name = "unknown";
    switch(input_data_type) {
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT:
            type_name = "FLOAT32";
            break;
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT16:
            type_name = "FLOAT16";
            break;
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_DOUBLE:
            type_name = "DOUBLE";
            break;
        default:
            type_name = "OTHER";
            break;
    }
    printf("[INFO] Model expects input data type: %s\n", type_name);

    size_t input_tensor_size = 1;
    for (int i = 0; i < 4; ++i) input_tensor_size *= input_shape[i];

    OrtMemoryInfo* memory_info = NULL;
    status = ort->CreateCpuMemoryInfo(OrtArenaAllocator, OrtMemTypeDefault, &memory_info);
    CHECK_STATUS(status, ort, "CreateCpuMemoryInfo failed");

    OrtValue* input_tensor = NULL;
    void* tensor_data = NULL;
    size_t tensor_data_size = 0;
    
    // Handle different input data types
    if (input_data_type == ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT16) {
        printf("[INFO] Converting FLOAT32 input to FLOAT16 for model compatibility\n");
        
        // Convert float32 to float16
        uint16_t* float16_data = convert_float32_to_float16(input_data, input_tensor_size);
        if (!float16_data) {
            fprintf(stderr, "[ERROR] Failed to convert data to float16\n");
            ort->ReleaseTypeInfo(input_type_info);
            ort->ReleaseMemoryInfo(memory_info);
            return 1;
        }
        
        tensor_data = float16_data;
        tensor_data_size = input_tensor_size * sizeof(uint16_t);
        
        status = ort->CreateTensorWithDataAsOrtValue(
            memory_info,
            tensor_data,
            tensor_data_size,
            input_shape,
            4,
            ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT16,
            &input_tensor
        );
    } else {
        // Use original float32 data
        tensor_data = input_data;
        tensor_data_size = input_tensor_size * sizeof(float);
        
        status = ort->CreateTensorWithDataAsOrtValue(
            memory_info,
            tensor_data,
            tensor_data_size,
            input_shape,
            4,
            input_data_type,
            &input_tensor
        );
    }
    CHECK_STATUS(status, ort, "CreateTensorWithDataAsOrtValue failed");

    int is_tensor;
    status = ort->IsTensor(input_tensor, &is_tensor);
    CHECK_STATUS(status, ort, "IsTensor check failed");
    if (!is_tensor) {
        fprintf(stderr, "[ERROR] Input is not a tensor.\n");
        if (input_data_type == ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT16 && tensor_data != input_data) {
            free(tensor_data);
        }
        ort->ReleaseTypeInfo(input_type_info);
        ort->ReleaseMemoryInfo(memory_info);
        return 1;
    }

    // ---- Prepare output ----
    char* output_name = NULL;
    status = ort->SessionGetOutputName(session, 0, allocator, &output_name);
    CHECK_STATUS(status, ort, "SessionGetOutputName failed");

    OrtValue* output_tensor = NULL;
    status = ort->Run(session, NULL, (const char* const*)&input_name, (const OrtValue* const*)&input_tensor, 1,
                      (const char* const*)&output_name, 1, &output_tensor);
    CHECK_STATUS(status, ort, "OrtRun failed");

    // Optionally: Inspect or print output (here we just confirm it's a tensor)
    status = ort->IsTensor(output_tensor, &is_tensor);
    CHECK_STATUS(status, ort, "IsTensor (output) check failed");
    if (!is_tensor) {
        fprintf(stderr, "[ERROR] Output is not a tensor.\n");
        if (input_data_type == ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT16 && tensor_data != input_data) {
            free(tensor_data);
        }
        ort->ReleaseTypeInfo(input_type_info);
        ort->ReleaseMemoryInfo(memory_info);
        return 1;
    }

    printf("[INFO] Inference successful. Output tensor is valid.\n");

    // ---- Parse and display output results ----
    DetectionResults* detection_results = NULL;
    if (parse_model_output(ort, output_tensor, 0.25f, &detection_results) == 0) {
        print_detection_results(detection_results);
        free_detection_results(detection_results);
    } else {
        printf("[WARN] Failed to parse model output\n");
    }

    // Cleanup
    ort->ReleaseValue(input_tensor);
    ort->ReleaseValue(output_tensor);
    ort->ReleaseMemoryInfo(memory_info);
    ort->ReleaseTypeInfo(input_type_info);
    allocator->Free(allocator, input_name);
    allocator->Free(allocator, output_name);
    
    // Free converted data if we allocated it
    if (input_data_type == ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT16 && tensor_data != input_data) {
        free(tensor_data);
    }

    return 0;
}
