#include <stdio.h>
#include <time.h>
#include <onnxruntime_c_api.h>
#include "onnx_utils.h"
#include "image_utils.h"

// ANSI escape codes for colors
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define RESET   "\033[0m"

int main() {
    const OrtApi* ort = OrtGetApiBase()->GetApi(ORT_API_VERSION);
    printf("ONNX Runtime API Version: %s\n", OrtGetApiBase()->GetVersionString());

    const char* model_path = "/home/pi/repos/embedded-object-detection-pi5/model/model.onnx";
    const char* image_path = "/home/pi/repos/embedded-object-detection-pi5/test_images/test.jpg";

    OrtEnv* env = NULL;
    OrtSession* session = NULL;

    printf(YELLOW "[INFO] Initializing ONNX Runtime environment...\n" RESET);
    if (init_env(ort, &env) != 0) {
        fprintf(stderr, RED "[ERROR] Failed to initialize ONNX Runtime environment.\n" RESET);
        return 1;
    }
    printf(GREEN "[OK] ONNX Runtime environment initialized.\n" RESET);

    printf(YELLOW "[INFO] Loading ONNX model from '%s'...\n" RESET, model_path);
    if (load_model(ort, env, model_path, &session) != 0) {
        fprintf(stderr, RED "[ERROR] Failed to load model or create session.\n" RESET);
        ort->ReleaseEnv(env);
        return 1;
    }
    printf(GREEN "[OK] Model loaded and session created.\n" RESET);

    float* input_tensor = NULL;
    int64_t input_shape[4] = {1, 3, 640, 640};  // Adjust for your model input

    printf(YELLOW "[INFO] Loading and preprocessing image from '%s'...\n" RESET, image_path);
    if (load_image_as_tensor(image_path, input_shape, &input_tensor) != 0) {
        fprintf(stderr, RED "[ERROR] Failed to load or preprocess input image.\n" RESET);
        ort->ReleaseSession(session);
        ort->ReleaseEnv(env);
        return 1;
    }
    printf(GREEN "[OK] Image loaded and preprocessed.\n" RESET);

    printf(YELLOW "[INFO] Running inference...\n" RESET);
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    if (run_inference(ort, session, input_tensor, input_shape) != 0) {
        fprintf(stderr, RED "[ERROR] Inference failed.\n" RESET);
        free(input_tensor);
        ort->ReleaseSession(session);
        ort->ReleaseEnv(env);
        return 1;
    }

    clock_gettime(CLOCK_MONOTONIC, &end);
    double elapsed = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
    printf(GREEN "[OK] Inference completed in %.3f seconds.\n" RESET, elapsed);

    // Cleanup
    free(input_tensor);
    ort->ReleaseSession(session);
    ort->ReleaseEnv(env);
    printf(GREEN "[DONE] All resources released. Exiting cleanly.\n" RESET);
    return 0;
}
