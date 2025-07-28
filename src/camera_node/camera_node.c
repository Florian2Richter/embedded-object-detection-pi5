#include "camera_node/camera_node.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <rcutils/logging_macros.h>
#include <rosidl_runtime_c/message_type_support_struct.h>

// Global flag for signal handling
static volatile sig_atomic_t g_running = 1;

void signal_handler(int sig) {
    (void)sig;
    g_running = 0;
}

int v4l2_open_device(camera_node_t* camera, const char* device) {
    camera->fd = open(device, O_RDWR);
    if (camera->fd == -1) {
        RCUTILS_LOG_ERROR("Cannot open device %s: %s", device, strerror(errno));
        return -1;
    }
    return 0;
}

int v4l2_init_device(camera_node_t* camera) {
    struct v4l2_capability cap;
    struct v4l2_format fmt;
    struct v4l2_requestbuffers req;
    struct v4l2_buffer buf;
    
    // Query device capabilities
    if (ioctl(camera->fd, VIDIOC_QUERYCAP, &cap) == -1) {
        RCUTILS_LOG_ERROR("VIDIOC_QUERYCAP failed: %s", strerror(errno));
        return -1;
    }
    
    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        RCUTILS_LOG_ERROR("Device does not support video capture");
        return -1;
    }
    
    if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
        RCUTILS_LOG_ERROR("Device does not support streaming I/O");
        return -1;
    }
    
    // Set video format
    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = CAMERA_WIDTH;
    fmt.fmt.pix.height = CAMERA_HEIGHT;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB24;
    fmt.fmt.pix.field = V4L2_FIELD_NONE;
    
    if (ioctl(camera->fd, VIDIOC_S_FMT, &fmt) == -1) {
        RCUTILS_LOG_ERROR("VIDIOC_S_FMT failed: %s", strerror(errno));
        return -1;
    }
    
    // Request buffers
    memset(&req, 0, sizeof(req));
    req.count = CAMERA_BUFFER_COUNT;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    
    if (ioctl(camera->fd, VIDIOC_REQBUFS, &req) == -1) {
        RCUTILS_LOG_ERROR("VIDIOC_REQBUFS failed: %s", strerror(errno));
        return -1;
    }
    
    if (req.count < 2) {
        RCUTILS_LOG_ERROR("Insufficient buffer memory");
        return -1;
    }
    
    camera->buffer_count = req.count;
    camera->buffers = calloc(req.count, sizeof(camera_buffer_t));
    
    if (!camera->buffers) {
        RCUTILS_LOG_ERROR("Out of memory");
        return -1;
    }
    
    // Map buffers
    for (int i = 0; i < camera->buffer_count; ++i) {
        memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        
        if (ioctl(camera->fd, VIDIOC_QUERYBUF, &buf) == -1) {
            RCUTILS_LOG_ERROR("VIDIOC_QUERYBUF failed: %s", strerror(errno));
            return -1;
        }
        
        camera->buffers[i].length = buf.length;
        camera->buffers[i].start = mmap(NULL, buf.length,
                                       PROT_READ | PROT_WRITE,
                                       MAP_SHARED,
                                       camera->fd, buf.m.offset);
        
        if (camera->buffers[i].start == MAP_FAILED) {
            RCUTILS_LOG_ERROR("mmap failed: %s", strerror(errno));
            return -1;
        }
    }
    
    return 0;
}

int v4l2_start_capture(camera_node_t* camera) {
    struct v4l2_buffer buf;
    enum v4l2_buf_type type;
    
    // Queue all buffers
    for (int i = 0; i < camera->buffer_count; ++i) {
        memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        
        if (ioctl(camera->fd, VIDIOC_QBUF, &buf) == -1) {
            RCUTILS_LOG_ERROR("VIDIOC_QBUF failed: %s", strerror(errno));
            return -1;
        }
    }
    
    // Start streaming
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(camera->fd, VIDIOC_STREAMON, &type) == -1) {
        RCUTILS_LOG_ERROR("VIDIOC_STREAMON failed: %s", strerror(errno));
        return -1;
    }
    
    camera->is_streaming = true;
    return 0;
}

int v4l2_stop_capture(camera_node_t* camera) {
    enum v4l2_buf_type type;
    
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(camera->fd, VIDIOC_STREAMOFF, &type) == -1) {
        RCUTILS_LOG_ERROR("VIDIOC_STREAMOFF failed: %s", strerror(errno));
        return -1;
    }
    
    camera->is_streaming = false;
    return 0;
}

int v4l2_read_frame(camera_node_t* camera) {
    struct v4l2_buffer buf;
    
    memset(&buf, 0, sizeof(buf));
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    
    // Dequeue buffer
    if (ioctl(camera->fd, VIDIOC_DQBUF, &buf) == -1) {
        if (errno == EAGAIN) {
            return 0; // No frame available
        }
        RCUTILS_LOG_ERROR("VIDIOC_DQBUF failed: %s", strerror(errno));
        return -1;
    }
    
    // Copy frame data to ROS message
    if (camera->image_msg && camera->buffers[buf.index].start) {
        size_t frame_size = CAMERA_WIDTH * CAMERA_HEIGHT * 3; // RGB24
        
        // Ensure message data is large enough
        if (camera->image_msg->data.capacity < frame_size) {
            sensor_msgs__msg__Image__fini(camera->image_msg);
            if (sensor_msgs__msg__Image__init(camera->image_msg) != RCL_RET_OK) {
                RCUTILS_LOG_ERROR("Failed to reinitialize image message");
                return -1;
            }
        }
        
        // Copy frame data
        memcpy(camera->image_msg->data.data, 
               camera->buffers[buf.index].start, 
               frame_size);
        
        camera->image_msg->data.size = frame_size;
        camera->image_msg->width = CAMERA_WIDTH;
        camera->image_msg->height = CAMERA_HEIGHT;
        camera->image_msg->step = CAMERA_WIDTH * 3;
        camera->image_msg->encoding.data = "rgb8";
        camera->image_msg->encoding.size = 4;
        camera->image_msg->encoding.capacity = 4;
    }
    
    // Re-queue buffer
    if (ioctl(camera->fd, VIDIOC_QBUF, &buf) == -1) {
        RCUTILS_LOG_ERROR("VIDIOC_QBUF failed: %s", strerror(errno));
        return -1;
    }
    
    return 1; // Frame captured
}

void v4l2_close_device(camera_node_t* camera) {
    if (camera->is_streaming) {
        v4l2_stop_capture(camera);
    }
    
    if (camera->buffers) {
        for (int i = 0; i < camera->buffer_count; ++i) {
            if (camera->buffers[i].start) {
                munmap(camera->buffers[i].start, camera->buffers[i].length);
            }
        }
        free(camera->buffers);
        camera->buffers = NULL;
    }
    
    if (camera->fd != -1) {
        close(camera->fd);
        camera->fd = -1;
    }
}

int camera_node_init(camera_node_t* camera, rcl_context_t* context) {
    rcl_ret_t ret;
    
    // Initialize camera structure
    memset(camera, 0, sizeof(camera_node_t));
    camera->fd = -1;
    
    // Initialize ROS2 node
    rcl_node_options_t node_options = rcl_node_get_default_options();
    ret = rcl_node_init(&camera->node, "camera_node", "", context, &node_options);
    if (ret != RCL_RET_OK) {
        RCUTILS_LOG_ERROR("Failed to initialize ROS2 node");
        return -1;
    }
    
    // Initialize publisher
    rcl_publisher_options_t pub_options = rcl_publisher_get_default_options();
    const rosidl_message_type_support_t* type_support = 
        ROSIDL_GET_MSG_TYPE_SUPPORT(sensor_msgs, msg, Image);
    
    ret = rcl_publisher_init(&camera->publisher, &camera->node, type_support, 
                            "/camera/image_raw", &pub_options);
    if (ret != RCL_RET_OK) {
        RCUTILS_LOG_ERROR("Failed to initialize publisher");
        rcl_node_fini(&camera->node);
        return -1;
    }
    

    
    // Initialize wait set (no timers, no subscriptions, just for publishing)
    ret = rcl_wait_set_init(&camera->wait_set, 0, 0, 0, 0, 0, 0, context, 
                           rcl_get_default_allocator());
    if (ret != RCL_RET_OK) {
        RCUTILS_LOG_ERROR("Failed to initialize wait set");
        rcl_publisher_fini(&camera->publisher, &camera->node);
        rcl_node_fini(&camera->node);
        return -1;
    }
    
    // Initialize image message
    camera->image_msg = sensor_msgs__msg__Image__create();
    if (!camera->image_msg) {
        RCUTILS_LOG_ERROR("Failed to create image message");
            rcl_wait_set_fini(&camera->wait_set);
    rcl_publisher_fini(&camera->publisher, &camera->node);
    rcl_node_fini(&camera->node);
        return -1;
    }
    
    // Initialize V4L2 camera
    if (v4l2_open_device(camera, CAMERA_DEVICE) != 0) {
        RCUTILS_LOG_ERROR("Failed to open V4L2 device");
        camera_node_fini(camera);
        return -1;
    }
    
    if (v4l2_init_device(camera) != 0) {
        RCUTILS_LOG_ERROR("Failed to initialize V4L2 device");
        camera_node_fini(camera);
        return -1;
    }
    
    if (v4l2_start_capture(camera) != 0) {
        RCUTILS_LOG_ERROR("Failed to start V4L2 capture");
        camera_node_fini(camera);
        return -1;
    }
    
    RCUTILS_LOG_INFO("Camera node initialized successfully");
    return 0;
}

void camera_node_fini(camera_node_t* camera) {
    if (camera->image_msg) {
        sensor_msgs__msg__Image__destroy(camera->image_msg);
        camera->image_msg = NULL;
    }
    
    rcl_wait_set_fini(&camera->wait_set);
    rcl_publisher_fini(&camera->publisher, &camera->node);
    rcl_node_fini(&camera->node);
    
    v4l2_close_device(camera);
}

int camera_node_spin(camera_node_t* camera) {
    rcl_ret_t ret;
    
    while (g_running) {
        // Read frame from camera
        int frame_result = v4l2_read_frame(camera);
        if (frame_result > 0 && camera->image_msg) {
            // Publish frame
            ret = rcl_publish(&camera->publisher, camera->image_msg, NULL);
            if (ret != RCL_RET_OK) {
                RCUTILS_LOG_ERROR("Failed to publish image");
            }
        }
        
        // Simple delay for 30 FPS
        usleep(33333); // ~30 FPS
    }
    
    return 0;
}

int main(int argc, char* argv[]) {
    // Set up signal handling
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Initialize RCL
    rcl_context_t context = rcl_get_zero_initialized_context();
    rcl_init_options_t init_options = rcl_get_zero_initialized_init_options();
    
    rcl_ret_t ret = rcl_init_options_init(&init_options, rcl_get_default_allocator());
    if (ret != RCL_RET_OK) {
        RCUTILS_LOG_ERROR("Failed to initialize init options");
        return 1;
    }
    
    ret = rcl_init(argc, (const char* const*)argv, &init_options, &context);
    if (ret != RCL_RET_OK) {
        RCUTILS_LOG_ERROR("Failed to initialize RCL");
        rcl_init_options_fini(&init_options);
        return 1;
    }
    
    // Initialize camera node
    camera_node_t camera;
    if (camera_node_init(&camera, &context) != 0) {
        RCUTILS_LOG_ERROR("Failed to initialize camera node");
        rcl_shutdown(&context);
        rcl_context_fini(&context);
        rcl_init_options_fini(&init_options);
        return 1;
    }
    
    RCUTILS_LOG_INFO("Camera node started");
    
    // Run camera node
    int result = camera_node_spin(&camera);
    
    // Cleanup
    camera_node_fini(&camera);
    rcl_shutdown(&context);
    rcl_context_fini(&context);
    rcl_init_options_fini(&init_options);
    
    RCUTILS_LOG_INFO("Camera node stopped");
    return result;
} 