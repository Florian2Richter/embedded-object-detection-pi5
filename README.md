# Embedded Object Detection on Raspberry Pi 5

A minimal, modular ROS2-C project for camera capture and display using V4L2 and SDL2. This project serves as a clean foundation for later ONNX inference integration.

## Requirements

### Prerequisites for Raspberry Pi 5
- **Ubuntu 24.04.2** - Operating system requirement for the Pi5
- **ROS2 Jazzy** - Required for ROS2 integration and node functionality

### Development Dependencies
- C compiler with C99 support (e.g., GCC)
- [CMake](https://cmake.org/) 3.8 or newer
- [SDL2](https://www.libsdl.org/) - For window and graphics
- V4L2 support (built into Linux kernel)

## Project Structure

```
embedded-object-detection-pi5/
├── include/
│   ├── camera_node/
│   │   └── camera_node.h          # Camera node header
│   └── display_node/
│       └── display_node.h         # Display node header
├── src/
│   ├── camera_node/
│   │   └── camera_node.c          # V4L2 camera capture node
│   └── display_node/
│       └── display_node.c         # SDL2 display node
├── CMakeLists.txt                 # Build configuration
├── package.xml                    # ROS2 package definition
└── README.md                      # This file
```

## Building

1. **Source ROS2 environment:**
   ```bash
   source /opt/ros/jazzy/setup.bash
   ```

2. **Build the project:**
   ```bash
   colcon build
   ```

3. **Source the workspace:**
   ```bash
   source install/setup.bash
   ```

## Usage

### Running the Camera Node
The camera node captures frames from `/dev/video0` using V4L2 and publishes them as ROS2 messages:

```bash
ros2 run embedded_object_detection_pi5 camera_node
```

**Features:**
- Captures 640x480 RGB24 frames at 30 FPS
- Publishes to `/camera/image_raw` topic
- Uses V4L2 memory-mapped buffers for efficiency
- Pure C implementation with ROS2 C API

### Running the Display Node
The display node subscribes to camera images and displays them in a window:

```bash
ros2 run embedded_object_detection_pi5 display_node
```

**Features:**
- Subscribes to `/camera/image_raw` topic
- Displays images in a resizable SDL2 window
- Supports multiple pixel formats (RGB24, BGR24, RGBA32, BGRA32)
- Pure C implementation with ROS2 C API

### Running Both Nodes
To see camera output in real-time:

**Terminal 1:**
```bash
ros2 run embedded_object_detection_pi5 camera_node
```

**Terminal 2:**
```bash
ros2 run embedded_object_detection_pi5 display_node
```

## Configuration

### Camera Settings
Edit `include/camera_node/camera_node.h` to modify:
- `CAMERA_DEVICE` - V4L2 device path (default: `/dev/video0`)
- `CAMERA_WIDTH` - Frame width (default: 640)
- `CAMERA_HEIGHT` - Frame height (default: 480)
- `CAMERA_FPS` - Frame rate (default: 30)
- `CAMERA_BUFFER_COUNT` - Number of V4L2 buffers (default: 4)

### Display Settings
Edit `include/display_node/display_node.h` to modify:
- `DISPLAY_WIDTH` - Window width (default: 640)
- `DISPLAY_HEIGHT` - Window height (default: 480)
- `DISPLAY_TITLE` - Window title (default: "Camera View")

## Troubleshooting

### Camera Issues
- **Permission denied:** Ensure your user has access to `/dev/video0`
  ```bash
  sudo usermod -a -G video $USER
  ```
- **Device not found:** Check available cameras:
  ```bash
  ls /dev/video*
  v4l2-ctl --list-devices
  ```

### Display Issues
- **SDL2 not found:** Install SDL2 development libraries:
  ```bash
  sudo apt install libsdl2-dev
  ```
- **No display:** Ensure you're running on a system with X11 or Wayland

## Architecture

This project follows a clean, modular design:

```
[USB Camera] → [V4L2] → [Camera Node] → [ROS2 Topic] → [Display Node] → [SDL2 Window]
```

### Key Design Principles
- **Pure C implementation** - No C++ dependencies
- **Modular structure** - Separate camera and display nodes
- **ROS2 idiomatic** - Uses standard ROS2 C API patterns
- **Beginner-friendly** - Clear separation of concerns
- **Extensible** - Easy to add ONNX inference later

## Next Steps

This clean foundation is ready for:
- **ONNX integration** - Add inference node for object detection
- **Image processing** - Add filters and transformations
- **Network streaming** - Add video streaming capabilities
- **Recording** - Add video recording functionality

## License

Apache 2.0 License - see LICENSE file for details.

