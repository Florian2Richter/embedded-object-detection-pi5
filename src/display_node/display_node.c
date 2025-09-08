#include "display_node/display_node.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <math.h>
#include <rcutils/logging_macros.h>
#include <rosidl_runtime_c/message_type_support_struct.h>

// Global flag for signal handling
static volatile sig_atomic_t g_running = 1;

void signal_handler(int sig) {
    (void)sig;
    g_running = 0;
}

// YUYV to RGB24 conversion function
void yuyv_to_rgb24(const uint8_t* yuyv_data, uint8_t* rgb_data, int width, int height) {
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x += 2) {
            int yuyv_idx = y * width * 2 + x * 2;
            int rgb_idx = y * width * 3 + x * 3;
            
            uint8_t y1 = yuyv_data[yuyv_idx];
            uint8_t u = yuyv_data[yuyv_idx + 1];
            uint8_t y2 = yuyv_data[yuyv_idx + 2];
            uint8_t v = yuyv_data[yuyv_idx + 3];
            
            // Convert YUV to RGB for first pixel
            int c1 = y1 - 16;
            int d = u - 128;
            int e = v - 128;
            
            int r1 = (298 * c1 + 409 * e + 128) >> 8;
            int g1 = (298 * c1 - 100 * d - 208 * e + 128) >> 8;
            int b1 = (298 * c1 + 516 * d + 128) >> 8;
            
            rgb_data[rgb_idx] = (uint8_t)(r1 < 0 ? 0 : (r1 > 255 ? 255 : r1));
            rgb_data[rgb_idx + 1] = (uint8_t)(g1 < 0 ? 0 : (g1 > 255 ? 255 : g1));
            rgb_data[rgb_idx + 2] = (uint8_t)(b1 < 0 ? 0 : (b1 > 255 ? 255 : b1));
            
            // Convert YUV to RGB for second pixel
            int c2 = y2 - 16;
            
            int r2 = (298 * c2 + 409 * e + 128) >> 8;
            int g2 = (298 * c2 - 100 * d - 208 * e + 128) >> 8;
            int b2 = (298 * c2 + 516 * d + 128) >> 8;
            
            rgb_data[rgb_idx + 3] = (uint8_t)(r2 < 0 ? 0 : (r2 > 255 ? 255 : r2));
            rgb_data[rgb_idx + 4] = (uint8_t)(g2 < 0 ? 0 : (g2 > 255 ? 255 : g2));
            rgb_data[rgb_idx + 5] = (uint8_t)(b2 < 0 ? 0 : (b2 > 255 ? 255 : b2));
        }
    }
}

int sdl2_init_window(display_node_t* display) {
    // Initialize SDL2
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        RCUTILS_LOG_ERROR("SDL could not initialize: %s", SDL_GetError());
        return -1;
    }
    
    // Create window
    display->window = SDL_CreateWindow(
        DISPLAY_TITLE,
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        DISPLAY_WIDTH,
        DISPLAY_HEIGHT,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );
    
    if (!display->window) {
        RCUTILS_LOG_ERROR("Failed to create SDL window: %s", SDL_GetError());
        SDL_Quit();
        return -1;
    }
    
    // Create renderer
    display->renderer = SDL_CreateRenderer(display->window, -1, SDL_RENDERER_ACCELERATED);
    if (!display->renderer) {
        RCUTILS_LOG_ERROR("Failed to create SDL renderer: %s", SDL_GetError());
        SDL_DestroyWindow(display->window);
        SDL_Quit();
        return -1;
    }
    
    // Create texture (will be updated with actual image data)
    display->texture = SDL_CreateTexture(
        display->renderer,
        SDL_PIXELFORMAT_RGB24,
        SDL_TEXTUREACCESS_STREAMING,
        DISPLAY_WIDTH,
        DISPLAY_HEIGHT
    );
    
    if (!display->texture) {
        RCUTILS_LOG_ERROR("Failed to create SDL texture: %s", SDL_GetError());
        SDL_DestroyRenderer(display->renderer);
        SDL_DestroyWindow(display->window);
        SDL_Quit();
        return -1;
    }
    
    return 0;
}

void sdl2_cleanup_window(display_node_t* display) {
    if (display->texture) {
        SDL_DestroyTexture(display->texture);
        display->texture = NULL;
    }
    
    if (display->renderer) {
        SDL_DestroyRenderer(display->renderer);
        display->renderer = NULL;
    }
    
    if (display->window) {
        SDL_DestroyWindow(display->window);
        display->window = NULL;
    }
    
    SDL_Quit();
}

int sdl2_update_display(display_node_t* display, const sensor_msgs__msg__Image* msg) {
    if (!msg || !msg->data.data || !display->texture) {
        return -1;
    }
    
    // Update texture with new image data
    void* pixels;
    int pitch;
    
    if (SDL_LockTexture(display->texture, NULL, &pixels, &pitch) != 0) {
        RCUTILS_LOG_ERROR("Failed to lock texture: %s", SDL_GetError());
        return -1;
    }
    
    // Check if we need to convert YUYV to RGB
    if (strcmp(msg->encoding.data, "yuv422_yuy2") == 0) {
        // Convert YUYV to RGB24
        yuyv_to_rgb24(msg->data.data, (uint8_t*)pixels, msg->width, msg->height);
    } else {
        // Direct copy for RGB formats
        memcpy(pixels, msg->data.data, msg->data.size);
    }
    
    SDL_UnlockTexture(display->texture);
    
    // Clear renderer and draw texture
    SDL_SetRenderDrawColor(display->renderer, 0, 0, 0, 255);
    SDL_RenderClear(display->renderer);
    
    SDL_RenderCopy(display->renderer, display->texture, NULL, NULL);
    SDL_RenderPresent(display->renderer);
    
    return 0;
}

void sdl2_handle_events(display_node_t* display) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT:
                RCUTILS_LOG_INFO("Window closed");
                display->is_running = false;
                g_running = 0;
                break;
                
            case SDL_KEYDOWN:
                if (event.key.keysym.sym == SDLK_ESCAPE || 
                    event.key.keysym.sym == SDLK_q) {
                    RCUTILS_LOG_INFO("Exit key pressed");
                    display->is_running = false;
                    g_running = 0;
                }
                break;
        }
    }
}

int display_node_init(display_node_t* display, rcl_context_t* context) {
    rcl_ret_t ret;
    
    // Initialize display structure
    memset(display, 0, sizeof(display_node_t));
    display->is_running = true;
    
    // Initialize SDL2 window
    if (sdl2_init_window(display) != 0) {
        RCUTILS_LOG_ERROR("Failed to initialize SDL2 window");
        return -1;
    }
    
    // Initialize ROS2 node
    rcl_node_options_t node_options = rcl_node_get_default_options();
    ret = rcl_node_init(&display->node, "display_node", "", context, &node_options);
    if (ret != RCL_RET_OK) {
        RCUTILS_LOG_ERROR("Failed to initialize ROS2 node");
        sdl2_cleanup_window(display);
        return -1;
    }
    
    // Initialize subscription
    rcl_subscription_options_t sub_options = rcl_subscription_get_default_options();
    const rosidl_message_type_support_t* type_support = 
        ROSIDL_GET_MSG_TYPE_SUPPORT(sensor_msgs, msg, Image);
    
    ret = rcl_subscription_init(&display->subscription, &display->node, type_support,
                               "/camera/image_raw", &sub_options);
    if (ret != RCL_RET_OK) {
        RCUTILS_LOG_ERROR("Failed to initialize subscription");
        rcl_node_fini(&display->node);
        sdl2_cleanup_window(display);
        return -1;
    }
    
    // Initialize wait set
    ret = rcl_wait_set_init(&display->wait_set, 1, 0, 0, 0, 0, 0, context,
                           rcl_get_default_allocator());
    if (ret != RCL_RET_OK) {
        RCUTILS_LOG_ERROR("Failed to initialize wait set");
        rcl_subscription_fini(&display->subscription, &display->node);
        rcl_node_fini(&display->node);
        sdl2_cleanup_window(display);
        return -1;
    }
    
    // Initialize image message
    display->image_msg = sensor_msgs__msg__Image__create();
    if (!display->image_msg) {
        RCUTILS_LOG_ERROR("Failed to create image message");
        rcl_wait_set_fini(&display->wait_set);
        rcl_subscription_fini(&display->subscription, &display->node);
        rcl_node_fini(&display->node);
        sdl2_cleanup_window(display);
        return -1;
    }
    
    RCUTILS_LOG_INFO("Display node initialized successfully");
    return 0;
}

void display_node_fini(display_node_t* display) {
    if (display->image_msg) {
        sensor_msgs__msg__Image__destroy(display->image_msg);
        display->image_msg = NULL;
    }
    
    rcl_wait_set_fini(&display->wait_set);
    rcl_subscription_fini(&display->subscription, &display->node);
    rcl_node_fini(&display->node);
    
    sdl2_cleanup_window(display);
}

int display_node_spin(display_node_t* display) {
    rcl_ret_t ret;
    
    while (g_running && display->is_running) {
        // Handle SDL events
        sdl2_handle_events(display);
        
        // Clear wait set
        ret = rcl_wait_set_clear(&display->wait_set);
        if (ret != RCL_RET_OK) {
            RCUTILS_LOG_ERROR("Failed to clear wait set");
            break;
        }
        
        // Add subscription to wait set
        ret = rcl_wait_set_add_subscription(&display->wait_set, &display->subscription, NULL);
        if (ret != RCL_RET_OK) {
            RCUTILS_LOG_ERROR("Failed to add subscription to wait set");
            break;
        }
        
        // Wait for messages (100ms timeout)
        ret = rcl_wait(&display->wait_set, RCL_MS_TO_NS(100));
        if (ret == RCL_RET_TIMEOUT) {
            // Timeout is expected, continue loop
            continue;
        } else if (ret != RCL_RET_OK) {
            RCUTILS_LOG_ERROR("Failed to wait on wait set");
            break;
        }
        
        // Check if subscription has data
        if (display->wait_set.subscriptions[0]) {
            // Take message
            rmw_message_info_t message_info;
            ret = rcl_take(&display->subscription, display->image_msg, &message_info, NULL);
            
            if (ret == RCL_RET_OK) {
                RCUTILS_LOG_DEBUG("Received image: %dx%d, encoding: %s", 
                    display->image_msg->width, display->image_msg->height, 
                    display->image_msg->encoding.data);
                
                // Update display
                sdl2_update_display(display, display->image_msg);
                
            } else if (ret != RCL_RET_SUBSCRIPTION_TAKE_FAILED) {
                RCUTILS_LOG_ERROR("Failed to take message");
            }
        }
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
    
    // Initialize display node
    display_node_t display;
    if (display_node_init(&display, &context) != 0) {
        RCUTILS_LOG_ERROR("Failed to initialize display node");
        rcl_shutdown(&context);
        rcl_context_fini(&context);
        rcl_init_options_fini(&init_options);
        return 1;
    }
    
    RCUTILS_LOG_INFO("Display node started");
    
    // Run display node
    int result = display_node_spin(&display);
    
    // Cleanup
    display_node_fini(&display);
    rcl_shutdown(&context);
    rcl_context_fini(&context);
    rcl_init_options_fini(&init_options);
    
    RCUTILS_LOG_INFO("Display node stopped");
    return result;
} 