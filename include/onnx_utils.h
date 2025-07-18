#ifndef ONNX_UTILS_H
#define ONNX_UTILS_H

#include <onnxruntime_c_api.h>

int load_model(const OrtApi* ort, const char* model_path);

#endif // ONNX_UTILS_H
