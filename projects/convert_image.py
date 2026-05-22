#!/usr/bin/env /usr/bin/python3
"""Convert PNG to monochrome C array for 5.83inch V2 e-Paper (648x480)."""
from PIL import Image
import os

# Config
INPUT = "/home/cedar/esp32/epaper/E-Paper_ESP32_Driver_Board_Code/墨稿-左右组合.png"
OUTPUT_DIR = "/home/cedar/esp32/projects/epaper_image_test"
W, H = 648, 480  # EPD_5in83_V2 resolution

os.makedirs(OUTPUT_DIR, exist_ok=True)

# Load image
img = Image.open(INPUT).convert("L")  # grayscale

# Resize: fit width, pad top/bottom with white
ratio = W / img.width
new_h = int(img.height * ratio)
img_resized = img.resize((W, new_h), Image.LANCZOS)

# Create white canvas and paste centered
canvas = Image.new("L", (W, H), 255)
y_offset = (H - new_h) // 2
canvas.paste(img_resized, (0, y_offset))

# Dither to 1-bit (Floyd-Steinberg)
img_1bit = canvas.convert("1", dither=Image.FLOYDSTEINBERG)

# Save preview
img_1bit.save(os.path.join(OUTPUT_DIR, "preview.png"))

# Convert to C array (1 bit per pixel, MSB first, row-major)
pixels = img_1bit.load()
width_bytes = W // 8  # 81

c_array = []
c_array.append(f"// Image: 墨稿-左右组合.png")
c_array.append(f"// Resolution: {W}x{H}, 1-bit monochrome")
c_array.append(f"const unsigned char gImage_custom[{W * H // 8}] = {{")

for y in range(H):
    row_data = []
    for x_byte in range(width_bytes):
        byte_val = 0
        for bit in range(8):
            px = pixels[x_byte * 8 + bit, y]
            # 0 = black, 255 = white in 1-bit mode
            # Our format: 1=white, 0=black (matching Waveshare convention)
            if px == 255:  # white
                byte_val |= (1 << (7 - bit))
        row_data.append(f"0x{byte_val:02X}")
    c_array.append("    " + ", ".join(row_data) + ",")

c_array.append("};")

# Write header file
header_path = os.path.join(OUTPUT_DIR, "ImageData.h")
with open(header_path, "w") as f:
    f.write("#ifndef _IMAGEDATA_H_\n")
    f.write("#define _IMAGEDATA_H_\n\n")
    f.write("extern const unsigned char gImage_custom[];\n\n")
    f.write("#endif\n")

# Write C file
c_path = os.path.join(OUTPUT_DIR, "ImageData.cpp")
with open(c_path, "w") as f:
    f.write('#include "ImageData.h"\n\n')
    f.write("\n".join(c_array))
    f.write("\n")

print(f"Done! Output in {OUTPUT_DIR}/")
print(f"  ImageData.h, ImageData.cpp, preview.png")
print(f"  Array size: {W * H // 8} bytes")
