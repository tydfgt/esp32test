#!/usr/bin/env /usr/bin/python3
"""
PNG → 5.83inch V2 e-Paper C数组 转换工具

用法：
  1. 修改下方 INPUT 为你的图片路径
  2. 修改 OUTPUT_DIR 为目标项目目录
  3. 运行: /usr/bin/python3 convert_image.py
  4. 编译烧录项目即可

依赖: python3-pil (系统包，已安装)
注意: 必须用 /usr/bin/python3，ESP-IDF 虚拟环境会屏蔽 PIL
"""

from PIL import Image
import os, sys

# ========== 配置区 ==========
INPUT = "/home/cedar/esp32/epaper/E-Paper_ESP32_Driver_Board_Code/墨稿-左右组合.png"
OUTPUT_DIR = "/home/cedar/esp32/projects/epaper_image_test"

# 屏幕参数（5.83inch V2）
W, H = 648, 480

# 缩放模式: "fit" = 等比+留白, "stretch" = 拉伸填满, "crop" = 等比裁切
SCALE_MODE = "fit"
# ============================

def convert_image(input_path, output_dir, w, h, mode="fit"):
    os.makedirs(output_dir, exist_ok=True)

    # 加载 → 灰度
    img = Image.open(input_path).convert("L")
    print(f"原图: {img.size}, 模式: {img.mode}")

    # 缩放
    if mode == "stretch":
        img_resized = img.resize((w, h), Image.LANCZOS)
        canvas = img_resized
    elif mode == "crop":
        ratio = max(w / img.width, h / img.height)
        new_w, new_h = int(img.width * ratio), int(img.height * ratio)
        img_resized = img.resize((new_w, new_h), Image.LANCZOS)
        left = (new_w - w) // 2
        top = (new_h - h) // 2
        canvas = img_resized.crop((left, top, left + w, top + h))
    else:  # fit (等比+留白居中)
        ratio = w / img.width
        new_h = int(img.height * ratio)
        img_resized = img.resize((w, new_h), Image.LANCZOS)
        canvas = Image.new("L", (w, h), 255)
        y_offset = (h - new_h) // 2
        canvas.paste(img_resized, (0, y_offset))

    # Floyd-Steinberg 抖动 → 1-bit
    img_1bit = canvas.convert("1", dither=Image.FLOYDSTEINBERG)
    img_1bit.save(os.path.join(output_dir, "preview.png"))
    print(f"预览已保存: {output_dir}/preview.png")

    # 生成 C 数组
    pixels = img_1bit.load()
    width_bytes = w // 8
    total_bytes = w * h // 8

    lines = []
    lines.append(f"// Auto-generated from: {os.path.basename(input_path)}")
    lines.append(f"// Resolution: {w}x{h}, 1-bit monochrome, mode={mode}")
    lines.append(f"const unsigned char gImage_custom[{total_bytes}] = {{")

    for y in range(h):
        row = []
        for xb in range(width_bytes):
            b = 0
            for bit in range(8):
                px = pixels[xb * 8 + bit, y]
                if px == 255:  # white
                    b |= (1 << (7 - bit))
            row.append(f"0x{b:02X}")
        lines.append("    " + ", ".join(row) + ",")
    lines.append("};")

    # 写 .h
    with open(os.path.join(output_dir, "ImageData.h"), "w") as f:
        f.write("#ifndef _IMAGEDATA_H_\n")
        f.write("#define _IMAGEDATA_H_\n\n")
        f.write("#include <arduino.h>\n\n")
        f.write("extern const unsigned char gImage_custom[];\n\n")
        f.write("#endif\n")

    # 写 .cpp
    with open(os.path.join(output_dir, "ImageData.cpp"), "w") as f:
        f.write('#include "ImageData.h"\n\n')
        f.write("\n".join(lines))
        f.write("\n")

    print(f"已生成: {output_dir}/ImageData.h")
    print(f"已生成: {output_dir}/ImageData.cpp")
    print(f"数组大小: {total_bytes} bytes ({total_bytes/1024:.1f} KB)")
    print("完成！现在可以编译烧录项目了。")

if __name__ == "__main__":
    if len(sys.argv) > 1:
        INPUT = sys.argv[1]
    if len(sys.argv) > 2:
        OUTPUT_DIR = sys.argv[2]
    if len(sys.argv) > 3:
        SCALE_MODE = sys.argv[3]

    if not os.path.exists(INPUT):
        print(f"错误: 图片不存在 - {INPUT}")
        sys.exit(1)

    convert_image(INPUT, OUTPUT_DIR, W, H, SCALE_MODE)
