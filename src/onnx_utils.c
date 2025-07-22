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

    // ---- Get input name and create tensor ----
    char* input_name = NULL;
    status = ort->SessionGetInputName(session, 0, allocator, &input_name);
    CHECK_STATUS(status, ort, "SessionGetInputName failed");

    size_t input_tensor_size = 1;
    for (int i = 0; i < 4; ++i) input_tensor_size *= input_shape[i];

    OrtMemoryInfo* memory_info = NULL;
    status = ort->CreateCpuMemoryInfo(OrtArenaAllocator, OrtMemTypeDefault, &memory_info);
    CHECK_STATUS(status, ort, "CreateCpuMemoryInfo failed");

    OrtValue* input_tensor = NULL;
    status = ort->CreateTensorWithDataAsOrtValue(
        memory_info,
        input_data,
        input_tensor_size * sizeof(float),
        input_shape,
        4,
        ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT,
        &input_tensor
    );
    CHECK_STATUS(status, ort, "CreateTensorWithDataAsOrtValue failed");

    int is_tensor;
    status = ort->IsTensor(input_tensor, &is_tensor);
    CHECK_STATUS(status, ort, "IsTensor check failed");
    if (!is_tensor) {
        fprintf(stderr, "[ERROR] Input is not a tensor.\n");
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
        return 1;
    }

    printf("[INFO] Inference successful. Output tensor is valid.\n");

    // Cleanup
    ort->ReleaseValue(input_tensor);
    ort->ReleaseValue(output_tensor);
    ort->ReleaseMemoryInfo(memory_info);
    allocator->Free(allocator, input_name);
    allocator->Free(allocator, output_name);

    return 0;
}
