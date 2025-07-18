cmake_minimum_required(VERSION 3.10)
project(embedded_object_detection)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set compilation flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -O3")

# Include directories
include_directories(include)

# Find required packages
find_package(OpenCV REQUIRED)

# Add executable
file(GLOB_RECURSE SOURCES "src/*.cpp" "src/*.c")
add_executable(object_detection ${SOURCES})

# Link libraries
target_link_libraries(object_detection ${OpenCV_LIBS})

# Optional: Add ONNX Runtime if available
# find_package(onnxruntime QUIET)
# if(onnxruntime_FOUND)
#     target_link_libraries(object_detection onnxruntime::onnxruntime)
#     target_compile_definitions(object_detection PRIVATE USE_ONNXRUNTIME)
# endif() 