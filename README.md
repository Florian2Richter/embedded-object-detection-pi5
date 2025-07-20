# Embedded Object Detection on Raspberry Pi 5

This project demonstrates how to run ONNX object detection models on a Raspberry Pi 5 using the **ONNX Runtime** C API.  The current code simply loads a model and verifies that the runtime is working, serving as a starting point for further development.

## Requirements

- C compiler with C11 support (e.g., GCC)
- [CMake](https://cmake.org/) 3.10 or newer
- Pre‑built ONNX Runtime library for aarch64.  By default the build expects it under:
  `$HOME/onnxruntime_c/onnxruntime-linux-aarch64-1.17.3`
- A trained ONNX model (placeholder path `model/model.onnx` is used in `main.c`)

## Building

1. Create a build directory and run CMake:
   ```bash
   mkdir build
   cd build
   cmake ..
   make
   ```
2. Run the example program:
   ```bash
   ./detector
   ```

Adjust `CMakeLists.txt` or set the `ONNX_DIR` variable if your ONNX Runtime installation is in a different location.

## Repository Layout

- `main.c` – Program entry point that prints the ONNX Runtime version and attempts to load the model.
- `src/onnx_utils.c` / `include/onnx_utils.h` – Helper functions to create an ONNX Runtime environment and session.
- `CMakeLists.txt` – Build configuration linking against ONNX Runtime.

The project is at an early stage and currently only tests that a model can be loaded.  Further object detection logic will be added in the future.
