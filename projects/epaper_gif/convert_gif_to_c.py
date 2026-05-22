#!/usr/bin/env /usr/bin/python3
"""
GIF → 5.83inch V2 e-Paper C数组 转换工具（多帧动画版）

用法：
  /usr/bin/python3 convert_gif_to_c.py <gif路径> <输出目录>

依赖: python3-pil (系统包)
注意: 必须用 /usr/bin/python3，ESP-IDF 虚拟环境会屏蔽 PIL
"""

from PIL import Image
import os, sys

# 屏幕参数（5.83inch V2）
SCR_W, SCR_H = 648, 480


def extract_gif_frames(gif_path):
    """提取GIF所有帧，正确处理delta帧合成"""
    gif = Image.open(gif_path)
    n_frames = 0
    try:
        while True:
            gif.seek(n_frames)
            n_frames += 1
    except EOFError:
        pass

    print(f"GIF: {gif.size}, {n_frames} 帧, 延迟 {gif.info.get('duration', '?')}ms")

    frames = []
    # 用上一帧的完整画布做合成基底（处理disposal）
    last_canvas = None

    for i in range(n_frames):
        gif.seek(i)
        # 转RGBA以获取alpha通道
        frame_rgba = gif.convert("RGBA")

        # 创建白底画布
        canvas = Image.new("RGBA", gif.size, (255, 255, 255, 255))

        # 如果GIF有disposal=restore_to_previous，需要先粘贴上一帧
        # 简单做法：每帧都paste到白底上，用alpha做mask
        if last_canvas is not None:
            # 先放上一帧的内容（处理disposal=none的情况）
            # 但最安全的做法是直接用当前帧+alpha合成到白底
            pass

        canvas.paste(frame_rgba, (0, 0), frame_rgba)
        # 转为RGB（去掉alpha），确保纯白背景
        canvas_rgb = Image.new("RGB", gif.size, (255, 255, 255))
        canvas_rgb.paste(canvas, (0, 0), canvas)

        last_canvas = canvas_rgb.copy()
        frames.append(canvas_rgb)

    return frames


def scale_to_content(img, scr_w, scr_h):
    """等比缩放到屏幕宽度，返回内容区域（不含白边）和Y偏移"""
    ratio = scr_w / img.width
    new_h = int(img.height * ratio)

    resized = img.resize((scr_w, new_h), Image.LANCZOS)

    y_offset = (scr_h - new_h) // 2
    return resized, new_h, y_offset


def image_to_bytes(img_1bit, w, h):
    """1-bit图像 → 字节数组（1=白, 0=黑, MSB first, 按行存储）"""
    pixels = img_1bit.load()
    width_bytes = w // 8
    data = bytearray()

    for y in range(h):
        for xb in range(width_bytes):
            b = 0
            for bit in range(8):
                px = pixels[xb * 8 + bit, y]
                if px == 255:  # white → bit=1
                    b |= (1 << (7 - bit))
            data.append(b)

    return bytes(data)


def convert_gif_to_c(gif_path, output_dir):
    os.makedirs(output_dir, exist_ok=True)

    # 1. 提取所有帧
    frames = extract_gif_frames(gif_path)
    n_frames = len(frames)

    # 2. 处理第一帧，确定缩放参数
    first_gray = frames[0].convert("L")
    scaled0, content_h, y_offset = scale_to_content(first_gray, SCR_W, SCR_H)
    print(f"缩放: {first_gray.size} → {SCR_W}×{content_h}, Y偏移={y_offset}")

    # 只存储内容区域（不含白边），减小flash占用和刷新时间
    CONTENT_W = SCR_W
    CONTENT_H = content_h

    # 3. 转换所有帧
    frame_data = []
    for i, frame in enumerate(frames):
        gray = frame.convert("L")
        scaled, _, _ = scale_to_content(gray, SCR_W, SCR_H)

        # Floyd-Steinberg 抖动 → 1-bit
        img_1bit = scaled.convert("1", dither=Image.FLOYDSTEINBERG)

        # 保存首帧预览
        if i == 0:
            preview = Image.new("L", (SCR_W, SCR_H), 255)
            preview.paste(img_1bit.convert("L"), (0, y_offset))
            preview.save(os.path.join(output_dir, "preview_frame_0.png"))
            print(f"首帧预览已保存")

        data = image_to_bytes(img_1bit, CONTENT_W, CONTENT_H)
        frame_data.append(data)

        if (i + 1) % 10 == 0 or i == n_frames - 1:
            print(f"  已处理 {i+1}/{n_frames} 帧")

    frame_bytes = len(frame_data[0])
    total = frame_bytes * n_frames
    print(f"每帧: {frame_bytes} bytes, 总计: {total} bytes ({total/1024:.1f} KB)")

    # 4. 生成 GifFrames.h
    h_content = f"""#ifndef _GIFFRAMES_H_
#define _GIFFRAMES_H_

#include <Arduino.h>

#define GIF_FRAME_COUNT {n_frames}
#define GIF_FRAME_W {CONTENT_W}
#define GIF_FRAME_H {CONTENT_H}
#define GIF_FRAME_BYTES {frame_bytes}
#define GIF_Y_OFFSET {y_offset}
#define GIF_X_OFFSET 0

extern const unsigned char gGifFrames[{n_frames}][{frame_bytes}];

#endif
"""
    with open(os.path.join(output_dir, "GifFrames.h"), "w") as f:
        f.write(h_content)
    print(f"已生成: GifFrames.h")

    # 5. 生成 GifFrames.cpp
    with open(os.path.join(output_dir, "GifFrames.cpp"), "w") as f:
        f.write('#include "GifFrames.h"\n\n')
        f.write(f'const unsigned char gGifFrames[{n_frames}][{frame_bytes}] PROGMEM = {{\n')

        for i, data in enumerate(frame_data):
            f.write(f"    {{ /* frame {i} */\n")
            # 每行16字节
            for row in range(0, len(data), 16):
                chunk = data[row:row+16]
                hex_str = ", ".join(f"0x{b:02X}" for b in chunk)
                f.write(f"        {hex_str},\n")
            f.write("    },\n")

        f.write("};\n")
    print(f"已生成: GifFrames.cpp ({total/1024:.1f} KB)")
    print("完成！现在可以编译烧录项目了。")


if __name__ == "__main__":
    if len(sys.argv) < 3:
        print(f"用法: /usr/bin/python3 {sys.argv[0]} <gif路径> <输出目录>")
        sys.exit(1)

    gif_path = sys.argv[1]
    output_dir = sys.argv[2]

    if not os.path.exists(gif_path):
        print(f"错误: 文件不存在 - {gif_path}")
        sys.exit(1)

    convert_gif_to_c(gif_path, output_dir)
