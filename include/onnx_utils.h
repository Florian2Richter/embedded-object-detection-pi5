#ifndef ONNX_UTILS_H
#define ONNX_UTILS_H

#include <onnxruntime_c_api.h>

int init_env(const OrtApi* ort, OrtEnv** env);
int load_model(const OrtApi* ort, OrtEnv* env, const char* model_path, OrtSession** session);
int run_inference(const OrtApi* ort, OrtSession* session, float* input_data, int64_t* input_shape);

#endif  // ONNX_UTILS_H
