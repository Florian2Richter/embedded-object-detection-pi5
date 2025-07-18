cmake_minimum_required(VERSION 3.10)
project(embedded_object_detection_pi5 C)

set(CMAKE_C_STANDARD 11)

# ONNX Runtime Pfade
set(ONNX_DIR "$ENV{HOME}/onnxruntime_c/onnxruntime-linux-aarch64-1.17.3")
include_directories(${ONNX_DIR}/include)
link_directories(${ONNX_DIR}/lib)

include_directories(include)
add_executable(detector main.c src/onnx_utils.c)
target_link_libraries(detector onnxruntime)

