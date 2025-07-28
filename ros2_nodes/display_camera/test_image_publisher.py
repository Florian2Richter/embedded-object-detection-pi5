#!/usr/bin/env python3

import rclpy
from rclpy.node import Node
from sensor_msgs.msg import Image
import numpy as np
import time

class TestImagePublisher(Node):
    def __init__(self):
        super().__init__('test_image_publisher')
        self.publisher = self.create_publisher(Image, '/camera/image_raw', 10)
        self.timer = self.create_timer(0.1, self.publish_image)  # 10 FPS
        self.get_logger().info('Test image publisher started')
        
    def publish_image(self):
        # Create a simple test image (640x480 RGB)
        width, height = 640, 480
        
        # Create a colorful pattern
        img = np.zeros((height, width, 3), dtype=np.uint8)
        
        # Create a moving color pattern
        t = time.time()
        for y in range(height):
            for x in range(width):
                # Create a moving wave pattern
                r = int(128 + 127 * np.sin(x * 0.01 + t))
                g = int(128 + 127 * np.sin(y * 0.01 + t))
                b = int(128 + 127 * np.sin((x + y) * 0.01 + t))
                
                img[y, x] = [r, g, b]
        
        # Create ROS Image message
        msg = Image()
        msg.header.stamp = self.get_clock().now().to_msg()
        msg.header.frame_id = 'camera_frame'
        msg.height = height
        msg.width = width
        msg.encoding = 'rgb8'
        msg.is_bigendian = False
        msg.step = width * 3  # 3 bytes per pixel
        msg.data = img.tobytes()
        
        self.publisher.publish(msg)
        self.get_logger().debug(f'Published image: {width}x{height}')

def main(args=None):
    rclpy.init(args=args)
    node = TestImagePublisher()
    
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()

if __name__ == '__main__':
    main() 