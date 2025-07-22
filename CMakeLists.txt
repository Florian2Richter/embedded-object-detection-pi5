cmake_minimum_required(VERSION 3.10)
project(embedded_object_detection_pi5 C)

set(CMAKE_C_STANDARD 11)

# ONNX Runtime Pfade
set(ONNX_DIR "$ENV{HOME}/onnxruntime_c/onnxruntime-linux-aarch64-1.17.3")
include_directories(${ONNX_DIR}/include)
link_directories(${ONNX_DIR}/lib)

find_package(OpenCV REQUIRED)

add_executable(detector
    main.c
    src/onnx_utils.c
    src/image_utils.c  
)

target_include_directories(detector PRIVATE
    ${OpenCV_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(detector PRIVATE ${OpenCV_LIBS} onnxruntime)
