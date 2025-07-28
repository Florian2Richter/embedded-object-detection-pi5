// Copyright 2024 Florian Richter
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>

// ROS 2 C API headers
#include <rcl/rcl.h>
#include <rcutils/logging_macros.h>
#include <sensor_msgs/msg/image.h>
#include <rosidl_runtime_c/message_type_support_struct.h>

// SDL2 headers
#include <SDL2/SDL.h>

// Global flag for signal handling
static volatile sig_atomic_t g_running = 1;

void signal_handler(int sig) {
    (void)sig;
    g_running = 0;
}

// Function to convert ROS Image message to SDL surface
SDL_Surface* ros_image_to_sdl_surface(const sensor_msgs__msg__Image* msg) {
    if (!msg || !msg->data.data) {
        return NULL;
    }

    // Determine pixel format based on encoding
    Uint32 pixel_format = SDL_PIXELFORMAT_RGB24;
    int bytes_per_pixel = 3;
    
    if (strcmp(msg->encoding.data, "rgb8") == 0) {
        pixel_format = SDL_PIXELFORMAT_RGB24;
        bytes_per_pixel = 3;
    } else if (strcmp(msg->encoding.data, "bgr8") == 0) {
        pixel_format = SDL_PIXELFORMAT_BGR24;
        bytes_per_pixel = 3;
    } else if (strcmp(msg->encoding.data, "mono8") == 0) {
        pixel_format = SDL_PIXELFORMAT_INDEX8;
        bytes_per_pixel = 1;
    } else if (strcmp(msg->encoding.data, "rgba8") == 0) {
        pixel_format = SDL_PIXELFORMAT_RGBA32;
        bytes_per_pixel = 4;
    } else if (strcmp(msg->encoding.data, "bgra8") == 0) {
        pixel_format = SDL_PIXELFORMAT_BGRA32;
        bytes_per_pixel = 4;
    } else {
        RCUTILS_LOG_WARN("Unsupported encoding: %s", msg->encoding.data);
        return NULL;
    }

    // Create SDL surface
    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormatFrom(
        (void*)msg->data.data,
        msg->width,
        msg->height,
        bytes_per_pixel * 8,
        msg->step,
        pixel_format
    );
    
    if (!surface) {
        RCUTILS_LOG_ERROR("Failed to create SDL surface: %s", SDL_GetError());
        return NULL;
    }

    return surface;
}

int main(int argc, char* argv[]) {
    // Set up signal handling
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        RCUTILS_LOG_ERROR("SDL could not initialize: %s", SDL_GetError());
        return 1;
    }

    // Initialize RCL
    rcl_context_t context = rcl_get_zero_initialized_context();
    rcl_init_options_t init_options = rcl_get_zero_initialized_init_options();
    
    rcl_ret_t ret = rcl_init_options_init(&init_options, rcl_get_default_allocator());
    if (ret != RCL_RET_OK) {
        RCUTILS_LOG_ERROR("Failed to initialize init options");
        SDL_Quit();
        return 1;
    }

    ret = rcl_init(argc, argv, &init_options, &context);
    if (ret != RCL_RET_OK) {
        RCUTILS_LOG_ERROR("Failed to initialize RCL");
        rcl_init_options_fini(&init_options);
        SDL_Quit();
        return 1;
    }

    // Create node
    rcl_node_t node = rcl_get_zero_initialized_node();
    rcl_node_options_t node_options = rcl_node_get_default_options();

    ret = rcl_node_init(&node, "image_display_node", "", &context, &node_options);
    if (ret != RCL_RET_OK) {
        RCUTILS_LOG_ERROR("Failed to initialize node");
        rcl_shutdown(&context);
        rcl_context_fini(&context);
        rcl_init_options_fini(&init_options);
        SDL_Quit();
        return 1;
    }

    RCUTILS_LOG_INFO("Image display node started");

    // Create subscription
    rcl_subscription_t subscription = rcl_get_zero_initialized_subscription();
    rcl_subscription_options_t subscription_options = rcl_subscription_get_default_options();

    const rosidl_message_type_support_t* type_support = 
        ROSIDL_GET_MSG_TYPE_SUPPORT(sensor_msgs, msg, Image);

    ret = rcl_subscription_init(
        &subscription,
        &node,
        type_support,
        "/camera/image_raw",
        &subscription_options
    );

    if (ret != RCL_RET_OK) {
        RCUTILS_LOG_ERROR("Failed to create subscription");
        rcl_node_fini(&node);
        rcl_shutdown(&context);
        rcl_context_fini(&context);
        rcl_init_options_fini(&init_options);
        SDL_Quit();
        return 1;
    }

    RCUTILS_LOG_INFO("Subscribed to /camera/image_raw");

    // Create SDL window
    SDL_Window* window = SDL_CreateWindow(
        "Camera View",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        640, 480,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );
    
    if (!window) {
        RCUTILS_LOG_ERROR("Failed to create SDL window: %s", SDL_GetError());
        rcl_subscription_fini(&subscription, &node);
        rcl_node_fini(&node);
        rcl_shutdown(&context);
        rcl_context_fini(&context);
        rcl_init_options_fini(&init_options);
        SDL_Quit();
        return 1;
    }

    // Create renderer
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        RCUTILS_LOG_ERROR("Failed to create SDL renderer: %s", SDL_GetError());
        SDL_DestroyWindow(window);
        rcl_subscription_fini(&subscription, &node);
        rcl_node_fini(&node);
        rcl_shutdown(&context);
        rcl_context_fini(&context);
        rcl_init_options_fini(&init_options);
        SDL_Quit();
        return 1;
    }

    // Create wait set
    rcl_wait_set_t wait_set = rcl_get_zero_initialized_wait_set();
    ret = rcl_wait_set_init(&wait_set, 1, 0, 0, 0, 0, 0, &context, rcl_get_default_allocator());
    if (ret != RCL_RET_OK) {
        RCUTILS_LOG_ERROR("Failed to create wait set");
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        rcl_subscription_fini(&subscription, &node);
        rcl_node_fini(&node);
        rcl_shutdown(&context);
        rcl_context_fini(&context);
        rcl_init_options_fini(&init_options);
        SDL_Quit();
        return 1;
    }

    // Allocate message
    sensor_msgs__msg__Image* msg = sensor_msgs__msg__Image__create();
    if (!msg) {
        RCUTILS_LOG_ERROR("Failed to allocate image message");
        rcl_wait_set_fini(&wait_set);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        rcl_subscription_fini(&subscription, &node);
        rcl_node_fini(&node);
        rcl_shutdown(&context);
        rcl_context_fini(&context);
        rcl_init_options_fini(&init_options);
        SDL_Quit();
        return 1;
    }

    RCUTILS_LOG_INFO("Starting main loop...");

    // Main loop
    while (g_running && rcl_context_is_valid(&context)) {
        // Handle SDL events
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                RCUTILS_LOG_INFO("Window closed");
                g_running = 0;
                break;
            } else if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.sym == SDLK_ESCAPE || 
                    event.key.keysym.sym == SDLK_q) {
                    RCUTILS_LOG_INFO("Exit key pressed");
                    g_running = 0;
                    break;
                }
            }
        }

        // Clear wait set
        ret = rcl_wait_set_clear(&wait_set);
        if (ret != RCL_RET_OK) {
            RCUTILS_LOG_ERROR("Failed to clear wait set");
            break;
        }

        // Add subscription to wait set
        ret = rcl_wait_set_add_subscription(&wait_set, &subscription, NULL);
        if (ret != RCL_RET_OK) {
            RCUTILS_LOG_ERROR("Failed to add subscription to wait set");
            break;
        }

        // Wait for messages (100ms timeout)
        ret = rcl_wait(&wait_set, RCL_MS_TO_NS(100));
        if (ret == RCL_RET_TIMEOUT) {
            // Timeout is expected, continue loop
            continue;
        } else if (ret != RCL_RET_OK) {
            RCUTILS_LOG_ERROR("Failed to wait on wait set");
            break;
        }

        // Check if subscription has data
        if (wait_set.subscriptions[0]) {
            // Take message
            rmw_message_info_t message_info;
            ret = rcl_take(&subscription, msg, &message_info, NULL);
            
            if (ret == RCL_RET_OK) {
                RCUTILS_LOG_DEBUG("Received image: %dx%d, encoding: %s", 
                    msg->width, msg->height, msg->encoding.data);

                // Convert ROS message to SDL surface
                SDL_Surface* surface = ros_image_to_sdl_surface(msg);
                if (surface) {
                    // Create texture from surface
                    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
                    if (texture) {
                        // Clear renderer
                        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
                        SDL_RenderClear(renderer);
                        
                        // Render texture
                        SDL_RenderCopy(renderer, texture, NULL, NULL);
                        SDL_RenderPresent(renderer);
                        
                        // Clean up texture
                        SDL_DestroyTexture(texture);
                    } else {
                        RCUTILS_LOG_ERROR("Failed to create texture: %s", SDL_GetError());
                    }
                    
                    // Free surface (this doesn't free the original data)
                    SDL_FreeSurface(surface);
                } else {
                    RCUTILS_LOG_WARN("Failed to convert ROS image to SDL format");
                }
            } else if (ret != RCL_RET_SUBSCRIPTION_TAKE_FAILED) {
                RCUTILS_LOG_ERROR("Failed to take message");
            }
        }
    }

    RCUTILS_LOG_INFO("Shutting down...");

    // Cleanup
    sensor_msgs__msg__Image__destroy(msg);
    rcl_wait_set_fini(&wait_set);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    rcl_subscription_fini(&subscription, &node);
    rcl_node_fini(&node);
    rcl_shutdown(&context);
    rcl_context_fini(&context);
    rcl_init_options_fini(&init_options);
    SDL_Quit();

    RCUTILS_LOG_INFO("Image display node stopped");
    return 0;
} 