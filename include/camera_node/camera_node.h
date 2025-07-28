#ifndef CAMERA_NODE_H
#define CAMERA_NODE_H

#include <stdint.h>
#include <stdbool.h>

// V4L2 includes
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

// ROS2 includes
#include <rcl/rcl.h>
#include <sensor_msgs/msg/image.h>

// Camera configuration
#define CAMERA_DEVICE "/dev/video0"
#define CAMERA_WIDTH 640
#define CAMERA_HEIGHT 480
#define CAMERA_FPS 30
#define CAMERA_BUFFER_COUNT 4

// Camera buffer structure
typedef struct {
    void* start;
    size_t length;
} camera_buffer_t;

// Camera node structure
typedef struct {
    int fd;                     // V4L2 device file descriptor
    camera_buffer_t* buffers;   // Mapped buffers
    int buffer_count;           // Number of buffers
    bool is_streaming;          // Streaming state
    
    // ROS2 components
    rcl_node_t node;
    rcl_publisher_t publisher;
    rcl_wait_set_t wait_set;
    
    // Image message
    sensor_msgs__msg__Image* image_msg;
} camera_node_t;

// Function declarations
int camera_node_init(camera_node_t* camera, rcl_context_t* context);
void camera_node_fini(camera_node_t* camera);
int camera_node_spin(camera_node_t* camera);

// V4L2 helper functions
int v4l2_open_device(camera_node_t* camera, const char* device);
int v4l2_init_device(camera_node_t* camera);
int v4l2_start_capture(camera_node_t* camera);
int v4l2_stop_capture(camera_node_t* camera);
int v4l2_read_frame(camera_node_t* camera);
void v4l2_close_device(camera_node_t* camera);

#endif // CAMERA_NODE_H 