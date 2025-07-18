#include <stdio.h>
#include <onnxruntime_c_api.h>

int main() {
    const OrtApi* ort = OrtGetApiBase()->GetApi(ORT_API_VERSION);
    printf("ONNX Runtime API Version: %s\n", OrtGetApiBase()->GetVersionString());
    return 0;
}
