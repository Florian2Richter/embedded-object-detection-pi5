#!/usr/bin/env python3
"""
Convert JPEG image to raw RGB format for pure C image loading.
Resizes to 640x640 and saves as raw RGB bytes.
"""

from PIL import Image
import numpy as np

def convert_jpeg_to_raw_rgb(input_path, output_path, target_size=(640, 640)):
    """Convert JPEG to raw RGB format."""
    try:
        # Load and convert image
        img = Image.open(input_path)
        print(f"Original image: {img.size} ({img.mode})")
        
        # Convert to RGB if not already
        if img.mode != 'RGB':
            img = img.convert('RGB')
        
        # Resize to target size
        img = img.resize(target_size, Image.Resampling.LANCZOS)
        print(f"Resized to: {img.size}")
        
        # Convert to numpy array and save as raw bytes
        rgb_array = np.array(img, dtype=np.uint8)
        print(f"Array shape: {rgb_array.shape}")
        
        # Save as raw RGB bytes (HWC format)
        with open(output_path, 'wb') as f:
            f.write(rgb_array.tobytes())
        
        print(f"Raw RGB data saved to: {output_path}")
        print(f"File size: {rgb_array.size} bytes")
        return True
        
    except Exception as e:
        print(f"Error: {e}")
        return False

if __name__ == "__main__":
    input_file = "test.jpg"
    output_file = "test.rgb"
    
    if convert_jpeg_to_raw_rgb(input_file, output_file):
        print("✅ Conversion successful!")
    else:
        print("❌ Conversion failed!") 