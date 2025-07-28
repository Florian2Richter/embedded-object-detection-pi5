cmake_minimum_required(VERSION 3.8)
project(embedded_object_detection_pi5)

# Set C standard
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Compiler flags
if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Find ROS2 packages
find_package(ament_cmake REQUIRED)
find_package(rcl REQUIRED)
find_package(rcutils REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(SDL2 REQUIRED)

# Include directories
include_directories(include)

# Camera Node
add_executable(camera_node 
  src/camera_node/camera_node.c
)

target_include_directories(camera_node PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)

target_compile_features(camera_node PUBLIC c_std_99)

ament_target_dependencies(camera_node
  rcl
  rcutils
  sensor_msgs)

target_link_libraries(camera_node SDL2::SDL2)

# Display Node
add_executable(display_node 
  src/display_node/display_node.c
)

target_include_directories(display_node PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)

target_compile_features(display_node PUBLIC c_std_99)

ament_target_dependencies(display_node
  rcl
  rcutils
  sensor_msgs)

target_link_libraries(display_node SDL2::SDL2)

# Install targets
install(TARGETS camera_node display_node
  DESTINATION lib/${PROJECT_NAME})

# Install headers
install(DIRECTORY include/
  DESTINATION include/
  FILES_MATCHING PATTERN "*.h")

ament_package()
