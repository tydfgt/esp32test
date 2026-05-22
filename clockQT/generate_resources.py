#!/usr/bin/env /usr/bin/python3
"""
墨水屏闹钟 — 资源生成器
生成: Logo C数组 + 大号数字位图(48x72) + 中文字模(32x32)

用法: /usr/bin/python3 generate_resources.py
"""

from PIL import Image, ImageFont, ImageDraw
import os, sys

OUTPUT_DIR = "/home/cedar/esp32/projects/epaper_clock"
LOGO_INPUT = "/home/cedar/esp32/clockQT/墨稿-左右组合.png"

# ==================== 1. Logo 转换 ====================
def convert_logo():
    print("\n=== 1. Logo → C数组 ===")
    logo = Image.open(LOGO_INPUT).convert("L")
    # 等比缩放到宽度500px
    target_w = 500
    ratio = target_w / logo.width
    target_h = int(logo.height * ratio)
    logo = logo.resize((target_w, target_h), Image.LANCZOS)

    # --- 全屏图 (logo在底部) ---
    canvas_full = Image.new("L", (648, 480), 255)
    x_off = (648 - target_w) // 2
    y_off = 480 - target_h - 12
    canvas_full.paste(logo, (x_off, y_off))
    img_1bit = canvas_full.convert("1", dither=Image.FLOYDSTEINBERG)
    img_1bit.save(os.path.join(OUTPUT_DIR, "preview_logo.png"))

    pixels = img_1bit.load()
    total_bytes = 648 * 480 // 8
    lines = ["// Logo全屏图: 墨稿-左右组合.png (648×480, Logo在底部)"]
    lines.append(f"const unsigned char gImage_logo[{total_bytes}] PROGMEM = {{")
    for y in range(480):
        row = []
        for xb in range(81):
            b = 0
            for bit in range(8):
                if pixels[xb*8 + bit, y] == 255:
                    b |= (1 << (7 - bit))
            row.append("0x%02X" % b)
        lines.append("    " + ", ".join(row) + ",")
    lines.append("};")

    with open(os.path.join(OUTPUT_DIR, "ImageData.h"), "w") as f:
        f.write("#ifndef _IMAGEDATA_H_\n#define _IMAGEDATA_H_\n\n")
        f.write("#include <Arduino.h>\n\n")
        f.write("// Logo全屏图: 648×480, 1-bit\n")
        f.write("extern const unsigned char gImage_logo[];\n")
        f.write(f"#define LOGO_Y {y_off}\n")
        f.write(f"#define LOGO_H {target_h}\n")
        f.write("#endif\n")

    with open(os.path.join(OUTPUT_DIR, "ImageData.cpp"), "w") as f:
        f.write('#include "ImageData.h"\n\n')
        f.write("\n".join(lines))
        f.write("\n")
    print(f"Logo全屏: {total_bytes} bytes ({total_bytes/1024:.1f} KB), y={y_off}, h={target_h}")

    # --- Logo-Only (仅logo区域, 用于部分刷新) ---
    logo_1bit = logo.convert("1", dither=Image.FLOYDSTEINBERG)
    logo_px = logo_1bit.load()
    logo_bytes = target_w * target_h // 8
    logo_lines = [f"// Logo-Only: {target_w}×{target_h}, 1-bit"]
    logo_lines.append(f"const unsigned char gImage_logoOnly[{logo_bytes}] PROGMEM = {{")
    for y in range(target_h):
        row = []
        for xb in range(target_w // 8):
            b = 0
            for bit in range(8):
                if logo_px[xb*8 + bit, y] == 255:
                    b |= (1 << (7 - bit))
            row.append("0x%02X" % b)
        logo_lines.append("    " + ", ".join(row) + ",")
    logo_lines.append("};")

    with open(os.path.join(OUTPUT_DIR, "LogoOnly.h"), "w") as f:
        f.write("#ifndef _LOGOONLY_H_\n#define _LOGOONLY_H_\n\n")
        f.write("#include <Arduino.h>\n\n")
        f.write(f"#define LOGO_ONLY_W {target_w}\n")
        f.write(f"#define LOGO_ONLY_H {target_h}\n")
        f.write(f"#define LOGO_ONLY_X {x_off}\n")
        f.write(f"#define LOGO_ONLY_Y {y_off}\n")
        f.write("extern const unsigned char gImage_logoOnly[];\n\n")
        f.write("#endif\n")

    with open(os.path.join(OUTPUT_DIR, "LogoOnly.cpp"), "w") as f:
        f.write('#include "LogoOnly.h"\n\n')
        f.write("\n".join(logo_lines))
        f.write("\n")
    print(f"Logo-Only: {logo_bytes} bytes, w={target_w}, h={target_h}")

# ==================== 2. 32×32 ASCII 数字/符号 ====================
ASCII32_W, ASCII32_H = 32, 32
ASCII32_CHARS = "0123456789%°:-/. "  # 常用ASCII字符
FONT_PATH = "/usr/share/fonts/truetype/wqy/wqy-microhei.ttc"

def generate_ascii32():
    print("\n=== 2. 32×32 ASCII位图 → C数组 ===")
    font = ImageFont.truetype(FONT_PATH, 32 * 3)

    lines_h = ["// 32×32 ASCII 位图 (与中文字模同高)"]
    lines_h.append("#ifndef _ASCII32DATA_H_\n#define _ASCII32DATA_H_\n")
    lines_h.append("#include <Arduino.h>\n")
    lines_h.append(f"#define ASCII32_W 32\n#define ASCII32_H 32")
    lines_h.append(f"#define ASCII32_BYTES 128")
    for ch in ASCII32_CHARS:
        name = "ascii32_sp" if ch == ' ' else f"ascii32_{ord(ch):02X}"
        lines_h.append(f"extern const unsigned char {name}[];")
    lines_h.append("const unsigned char* getAscii32Bitmap(char c);")
    lines_h.append("#endif\n")

    lines_cpp = ['#include "Ascii32Data.h"\n']
    char_data = {}

    for ch in ASCII32_CHARS:
        img = Image.new("L", (32 * 3, 32 * 3), 255)
        draw = ImageDraw.Draw(img)
        if ch == ' ':
            # 空格留空
            pass
        else:
            bbox = font.getbbox(ch)
            tw = bbox[2] - bbox[0]
            th = bbox[3] - bbox[1]
            draw.text(((img.width - tw) // 2 - bbox[0], (img.height - th) // 2 - bbox[1]),
                      ch, font=font, fill=0)
        img = img.resize((32, 32), Image.LANCZOS)
        img = img.point(lambda p: 255 if p > 128 else 0)
        pixels = img.load()

        data = []
        for y in range(32):
            for xb in range(4):
                b = 0
                for bit in range(8):
                    if pixels[xb*8 + bit, y] >= 128:
                        b |= (1 << (7 - bit))
                data.append(b)

        name = "ascii32_sp" if ch == ' ' else f"ascii32_{ord(ch):02X}"
        hex_str = ", ".join("0x%02X" % b for b in data)
        lines_cpp.append(f"// '{ch}'")
        lines_cpp.append(f"const unsigned char {name}[ASCII32_BYTES] PROGMEM = {{")
        lines_cpp.append(f"    {hex_str}")
        lines_cpp.append("};")
        lines_cpp.append("")
        char_data[ch] = name

    # 查找函数
    lines_cpp.append("const unsigned char* getAscii32Bitmap(char c) {")
    lines_cpp.append("    switch (c) {")
    for ch in ASCII32_CHARS:
        name = "ascii32_sp" if ch == ' ' else f"ascii32_{ord(ch):02X}"
        if ch == '\'':
            lines_cpp.append(f"        case '\\'': return {name};")
        else:
            lines_cpp.append(f"        case '{ch}': return {name};")
    lines_cpp.append("        default: return ascii32_sp;")
    lines_cpp.append("    }")
    lines_cpp.append("}")

    with open(os.path.join(OUTPUT_DIR, "Ascii32Data.h"), "w") as f:
        f.write("\n".join(lines_h) + "\n")
    with open(os.path.join(OUTPUT_DIR, "Ascii32Data.cpp"), "w") as f:
        f.write("\n".join(lines_cpp) + "\n")
    print(f"ASCII32: {len(ASCII32_CHARS)} 字符, 每字符 128 字节")

# ==================== 3. 大号数字位图 (时间用) ====================
DIGIT_W, DIGIT_H = 84, 148

def generate_digits():
    print("\n=== 3. 大号数字 → C数组 ===")
    font = ImageFont.truetype(FONT_PATH, 174)

    lines_h = ["// 大号数字位图 (%d×%d), 用于时钟时间显示" % (DIGIT_W, DIGIT_H)]
    lines_h.append("#ifndef _DIGITDATA_H_\n#define _DIGITDATA_H_\n")
    lines_h.append("#include <Arduino.h>\n")
    lines_h.append("#define DIGIT_W %d\n#define DIGIT_H %d" % (DIGIT_W, DIGIT_H))
    lines_h.append("#define DIGIT_BYTES %d" % (DIGIT_W * DIGIT_H // 8))
    for i in range(10):
        lines_h.append(f"extern const unsigned char g_digit_{i}[];")
    lines_h.append("extern const unsigned char g_colon_bitmap[];")
    lines_h.append("// 根据字符获取对应位图指针")
    lines_h.append("inline const unsigned char* getDigitBitmap(char c) {")
    lines_h.append("    if (c >= '0' && c <= '9') {")
    lines_h.append("        static const unsigned char* const tbl[] = {")
    for i in range(10):
        lines_h.append(f"            g_digit_{i},")
    lines_h.append("        };")
    lines_h.append("        return tbl[c - '0'];")
    lines_h.append("    }")
    lines_h.append("    return nullptr;")
    lines_h.append("}")
    lines_h.append("#endif\n")

    lines_cpp = ['#include "DigitData.h"\n']

    for ch in "0123456789:":
        img = Image.new("L", (DIGIT_W * 3, DIGIT_H * 3), 255)
        draw = ImageDraw.Draw(img)
        bbox = font.getbbox(ch)
        text_w = bbox[2] - bbox[0]
        text_h = bbox[3] - bbox[1]
        draw.text(((img.width - text_w) // 2 - bbox[0], (img.height - text_h) // 2 - bbox[1]),
                  ch, font=font, fill=0)
        img = img.resize((DIGIT_W, DIGIT_H), Image.LANCZOS)
        img = img.point(lambda p: 255 if p > 128 else 0)
        pixels = img.load()

        byte_count = DIGIT_W * DIGIT_H // 8
        data = []
        for y in range(DIGIT_H):
            for xb in range(DIGIT_W // 8):
                b = 0
                for bit in range(8):
                    if pixels[xb*8 + bit, y] >= 128:
                        b |= (1 << (7 - bit))
                data.append(b)

        hex_str = ", ".join("0x%02X" % b for b in data)
        name = "g_colon_bitmap" if ch == ":" else f"g_digit_{ch}"
        lines_cpp.append(f"// '{ch}'")
        lines_cpp.append(f"const unsigned char {name}[DIGIT_BYTES] PROGMEM = {{")
        lines_cpp.append(f"    {hex_str}")
        lines_cpp.append("};")
        lines_cpp.append("")

    with open(os.path.join(OUTPUT_DIR, "DigitData.h"), "w") as f:
        f.write("\n".join(lines_h))
    with open(os.path.join(OUTPUT_DIR, "DigitData.cpp"), "w") as f:
        f.write("\n".join(lines_cpp))
    print(f"数字位图: {DIGIT_W}×{DIGIT_H}, 每字符{byte_count}字节")

# ==================== 4. 中文字模 ====================
def generate_cn_font():
    print("\n=== 3. 中文字模 → C数组 ===")
    chars = "广州增城新塘北京东南西北天气晴多云阴雨阵雪雾霾年月日周一二三四五六七八九时间级度湿温风力更获取中加载失败无网络"
    charset = list(dict.fromkeys(chars))  # 去重保序
    print(f"字符集 ({len(charset)}): {''.join(charset)}")

    font = ImageFont.truetype(FONT_PATH, 32 * 3)  # 3x 抗锯齿

    table_entries = []
    for ch in charset:
        img = Image.new("L", (32 * 3, 32 * 3), 255)
        draw = ImageDraw.Draw(img)
        bbox = font.getbbox(ch)
        tw = bbox[2] - bbox[0]
        th = bbox[3] - bbox[1]
        draw.text(((img.width - tw) // 2 - bbox[0], (img.height - th) // 2 - bbox[1]),
                  ch, font=font, fill=0)
        img = img.resize((32, 32), Image.LANCZOS)
        img = img.point(lambda p: 255 if p > 128 else 0)
        pixels = img.load()

        bitmap = []
        for y in range(32):
            b = 0
            for bit in range(8):
                x = bit
                if pixels[x, y] <= 128:
                    b |= (1 << (7 - bit))
            bitmap.append(b)
            b = 0
            for bit in range(8):
                x = 8 + bit
                if pixels[x, y] <= 128:
                    b |= (1 << (7 - bit))
            bitmap.append(b)
            b = 0
            for bit in range(8):
                x = 16 + bit
                if pixels[x, y] <= 128:
                    b |= (1 << (7 - bit))
            bitmap.append(b)
            b = 0
            for bit in range(8):
                x = 24 + bit
                if pixels[x, y] <= 128:
                    b |= (1 << (7 - bit))
            bitmap.append(b)

        utf8 = list(ch.encode('utf-8'))
        while len(utf8) < 3:
            utf8.append(0)
        idx_hex = "0x%02X, 0x%02X, 0x%02X" % (utf8[0], utf8[1], utf8[2])
        hex_str = ", ".join("0x%02X" % b for b in bitmap)
        table_entries.append("    {{%s},\n     {%s}}" % (idx_hex, hex_str))

    lines_h = ["#ifndef __FONT24CN_CLOCK_H", "#define __FONT24CN_CLOCK_H",
               '#include "fonts.h"', "extern cFONT Font24CN_Clock;", "#endif"]

    sep = ",\n"
    lines_cpp = [
        '#include "Font24CN_Clock.h"',
        f"const CH_CN Font24CN_Clock_Table[] = {{\n{sep.join(table_entries)}\n}};",
        "cFONT Font24CN_Clock = {",
        "    Font24CN_Clock_Table,",
        f"    sizeof(Font24CN_Clock_Table) / sizeof(CH_CN),",
        "    16,  /* ASCII Width */",
        "    32,  /* Width */",
        "    32,  /* Height */",
        "};"
    ]

    with open(os.path.join(OUTPUT_DIR, "Font24CN_Clock.h"), "w") as f:
        f.write("\n".join(lines_h) + "\n")
    with open(os.path.join(OUTPUT_DIR, "Font24CN_Clock.cpp"), "w") as f:
        f.write("\n".join(lines_cpp) + "\n")
    print(f"字模: {len(charset)} 字符, {len(charset)*128} 字节")

# ==================== Main ====================
if __name__ == "__main__":
    os.makedirs(OUTPUT_DIR, exist_ok=True)
    convert_logo()
    generate_ascii32()
    generate_digits()
    generate_cn_font()
    print("\n=== 全部资源生成完成! ===")
    print(f"输出目录: {OUTPUT_DIR}")
