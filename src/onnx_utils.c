#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "onnx_utils.h"

int load_model(const OrtApi* ort, const char* model_path) {
    OrtEnv* env;
    OrtStatus* status = ort->CreateEnv(ORT_LOGGING_LEVEL_WARNING, "test", &env);
    if (status != NULL) {
        fprintf(stderr, "Failed to create OrtEnv\n");
        return -1;
    }

    OrtSessionOptions* session_options;
    ort->CreateSessionOptions(&session_options);
    ort->SetIntraOpNumThreads(session_options, 1);

    OrtSession* session;
    status = ort->CreateSession(env, model_path, session_options, &session);
    if (status != NULL) {
        fprintf(stderr, "Failed to load ONNX model: %s\n", model_path);
        ort->ReleaseSessionOptions(session_options);
        ort->ReleaseEnv(env);
        return -1;
    }

    printf("ONNX model loaded successfully: %s\n", model_path);

    // Clean up
    ort->ReleaseSession(session);
    ort->ReleaseSessionOptions(session_options);
    ort->ReleaseEnv(env);

    return 0;
}
