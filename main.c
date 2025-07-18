#include <stdio.h>
#include <onnxruntime_c_api.h>
#include "onnx_utils.h"

int main() {
    const OrtApi* ort = OrtGetApiBase()->GetApi(ORT_API_VERSION);
    printf("ONNX Runtime API Version: %s\n", OrtGetApiBase()->GetVersionString());

    const char* model_path = "model/model.onnx";  // Platzhalter
    if (load_model(ort, model_path) != 0) {
        fprintf(stderr, "Model loading failed.\n");
        return 1;
    }

    return 0;
}
