#ifndef DISPLAY_NODE_H
#define DISPLAY_NODE_H

#include <stdint.h>
#include <stdbool.h>

// SDL2 includes
#include <SDL2/SDL.h>

// ROS2 includes
#include <rcl/rcl.h>
#include <sensor_msgs/msg/image.h>

// Display configuration
#define DISPLAY_WIDTH 640
#define DISPLAY_HEIGHT 480
#define DISPLAY_TITLE "Camera View"

// Display node structure
typedef struct {
    // SDL2 components
    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_Texture* texture;
    
    // ROS2 components
    rcl_node_t node;
    rcl_subscription_t subscription;
    rcl_wait_set_t wait_set;
    
    // Image message
    sensor_msgs__msg__Image* image_msg;
    
    // State
    bool is_running;
} display_node_t;

// Function declarations
int display_node_init(display_node_t* display, rcl_context_t* context);
void display_node_fini(display_node_t* display);
int display_node_spin(display_node_t* display);

// SDL2 helper functions
int sdl2_init_window(display_node_t* display);
void sdl2_cleanup_window(display_node_t* display);
int sdl2_update_display(display_node_t* display, const sensor_msgs__msg__Image* msg);
void sdl2_handle_events(display_node_t* display);

#endif // DISPLAY_NODE_H 