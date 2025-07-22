#ifndef IMAGE_UTILS_H
#define IMAGE_UTILS_H

// Loads and preprocesses an image (resized to input_shape[2] x input_shape[3]).
// Expects input_shape to be {1, 3, H, W} and fills output with malloc'd float* (CHW format).
// Returns 0 on success, 1 on failure.
int load_image_as_tensor(const char* image_path, const int64_t* input_shape, float** output);

#endif  // IMAGE_UTILS_H
