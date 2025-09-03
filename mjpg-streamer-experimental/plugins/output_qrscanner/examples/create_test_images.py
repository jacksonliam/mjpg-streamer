#!/usr/bin/env python3
"""
Test the QR scanner plugin by creating a simple JPEG test image
This script creates a small JPEG with text that can be used to test the JPEG decoder
"""

from PIL import Image, ImageDraw, ImageFont
import io
import sys

def create_test_jpeg():
    """Create a simple test JPEG image"""
    # Create a 200x200 grayscale image
    img = Image.new('L', (200, 200), color=255)
    draw = ImageDraw.Draw(img)

    # Draw some test patterns
    draw.rectangle([10, 10, 190, 190], outline=0, width=3)
    draw.rectangle([30, 30, 170, 170], outline=128, width=2)
    draw.rectangle([50, 50, 150, 150], outline=64, width=1)

    # Add text
    try:
        font = ImageFont.load_default()
        draw.text((60, 90), "TEST", fill=0, font=font)
        draw.text((55, 110), "JPEG", fill=0, font=font)
    except:
        # Fallback if font loading fails
        draw.text((60, 90), "TEST", fill=0)
        draw.text((55, 110), "JPEG", fill=0)

    # Save as JPEG
    output = io.BytesIO()
    img.save(output, format='JPEG', quality=85)
    jpeg_data = output.getvalue()

    # Also save to file for inspection
    with open('test_image.jpg', 'wb') as f:
        f.write(jpeg_data)

    print(f"Created test JPEG: {len(jpeg_data)} bytes")
    print("Saved as test_image.jpg")

    return jpeg_data

def create_qr_test_image():
    """Create a test image with a QR code pattern (not a real QR code)"""
    # Create a simple pattern that looks like a QR code
    img = Image.new('L', (200, 200), color=255)
    draw = ImageDraw.Draw(img)

    # Draw finder patterns (corners)
    for x, y in [(20, 20), (160, 20), (20, 160)]:
        # Outer square
        draw.rectangle([x, y, x+20, y+20], fill=0)
        # Inner white square
        draw.rectangle([x+2, y+2, x+18, y+18], fill=255)
        # Inner black square
        draw.rectangle([x+6, y+6, x+14, y+14], fill=0)

    # Draw some data pattern
    for i in range(8, 15):
        for j in range(8, 15):
            if (i + j) % 2 == 0:
                draw.rectangle([20 + i*8, 60 + j*8, 20 + i*8 + 6, 60 + j*8 + 6], fill=0)

    # Save as JPEG
    output = io.BytesIO()
    img.save(output, format='JPEG', quality=90)
    jpeg_data = output.getvalue()

    with open('test_qr_pattern.jpg', 'wb') as f:
        f.write(jpeg_data)

    print(f"Created QR-like test pattern: {len(jpeg_data)} bytes")
    print("Saved as test_qr_pattern.jpg")

    return jpeg_data

if __name__ == "__main__":
    try:
        print("Creating test images for QR scanner plugin...")
        create_test_jpeg()
        create_qr_test_image()
        print("\nTest images created successfully!")
        print("You can use these to test the JPEG decoder functionality.")
    except ImportError:
        print("Error: PIL (Pillow) library not found")
        print("Install it with: pip install Pillow")
        sys.exit(1)
    except Exception as e:
        print(f"Error creating test images: {e}")
        sys.exit(1)
