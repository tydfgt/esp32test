#!/usr/bin/env /usr/bin/python3
"""
墨水屏桌面闹钟 — 资源生成脚本
生成: Logo C数组 / 大号数字位图(48x72) / 中文字模(32x32)
"""

from PIL import Image, ImageFont, ImageDraw
import os, sys, struct

OUTPUT_DIR = "/home/cedar/esp32/projects/epaper_clock_qt"
LOGO_INPUT = "/home/cedar/esp32/clockQT/墨稿-左右组合.png"
W, H = 648, 480

os.makedirs(OUTPUT_DIR, exist_ok=True)

# ============================================================
# 1. Logo 转换 (宽600, 等比缩放到底部约46px高)
# ============================================================
print("=" * 50)
print("1. 转换 Logo...")

logo_img = Image.open(LOGO_INPUT).convert("L")
logo_target_w = 560
ratio = logo_target_w / logo_img.width
logo_target_h = int(logo_img.height * ratio)
logo_resized = logo_img.resize((logo_target_w, logo_target_h), Image.LANCZOS)

# Floyd-Steinberg 抖动
logo_1bit = logo_resized.convert("1", dither=Image.FLOYDSTEINBERG)
logo_1bit.save(os.path.join(OUTPUT_DIR, "preview_logo.png"))

# 生成 C 数组 (1=白, 0=黑)
pixels = logo_1bit.load()
lw = logo_target_w
lh = logo_target_h
lb = (lw + 7) // 8

logo_lines = []
logo_lines.append(f"// Logo: {lw}x{lh}, 1-bit")
logo_lines.append(f"const unsigned char gImage_logo[{lh * lb}] PROGMEM = {{")
for y in range(lh):
    row = []
    for xb in range(lb):
        b = 0
        for bit in range(8):
            px_x = xb * 8 + bit
            if px_x < lw:
                px = pixels[px_x, y]
                if px == 255:
                    b |= (1 << (7 - bit))
        row.append(f"0x{b:02X}")
    logo_lines.append("    " + ", ".join(row) + ",")
logo_lines.append("};")

with open(os.path.join(OUTPUT_DIR, "LogoData.h"), "w") as f:
    f.write("#pragma once\n#include <Arduino.h>\n")
    f.write(f"#define LOGO_W {lw}\n#define LOGO_H {lh}\n")
    f.write("extern const unsigned char gImage_logo[];\n")

with open(os.path.join(OUTPUT_DIR, "LogoData.cpp"), "w") as f:
    f.write('#include "LogoData.h"\n\n')
    f.write("\n".join(logo_lines) + "\n")

print(f"   Logo: {lw}x{lh}, {lh*lb} bytes")

# ============================================================
# 2. 大号数字位图 48x72 (0-9 + 冒号)
# ============================================================
print("2. 生成大号数字位图 (48x72)...")

try:
    digit_font = ImageFont.truetype("/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf", 64)
except:
    try:
        digit_font = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 64)
    except:
        digit_font = ImageFont.load_default()

DW, DH = 48, 72
chars = list("0123456789") + [":"]
digit_data = {}

for ch in chars:
    # 3x 超采样抗锯齿
    scale = 3
    img = Image.new("L", (DW * scale, DH * scale), 255)
    draw = ImageDraw.Draw(img)
    text = ":" if ch == ":" else ch
    bbox = draw.textbbox((0, 0), text, font=digit_font)
    tw = bbox[2] - bbox[0]
    th = bbox[3] - bbox[1]
    x = (DW * scale - tw) // 2 - bbox[0]
    y = (DH * scale - th) // 2 - bbox[1]
    draw.text((x, y), text, font=digit_font, fill=0)
    img = img.resize((DW, DH), Image.LANCZOS)

    # 二值化 (阈值128)
    img_bin = img.point(lambda p: 255 if p > 128 else 0).convert("1")
    px = img_bin.load()

    db = (DW + 7) // 8
    rows = []
    for row_y in range(DH):
        vals = []
        for xb in range(db):
            b = 0
            for bit in range(8):
                xx = xb * 8 + bit
                if xx < DW and px[xx, row_y] == 255:
                    b |= (1 << (7 - bit))
            vals.append(f"0x{b:02X}")
        rows.append("    " + ", ".join(vals) + ",")

    key = "d_" + ("colon" if ch == ":" else ch)
    digit_data[key] = {
        "w": DW,
        "h": DH,
        "bytes": db * DH,
        "code": "\n".join(rows)
    }
    print(f"   '{ch}' -> {DW}x{DH}, {db*DH} bytes")

# 写入 DigitData.h
with open(os.path.join(OUTPUT_DIR, "DigitData.h"), "w") as f:
    f.write("#pragma once\n#include <Arduino.h>\n\n")
    f.write(f"#define DIGIT_W {DW}\n#define DIGIT_H {DH}\n\n")
    for key in sorted(digit_data.keys()):
        f.write(f"extern const unsigned char gDigit_{key}[];\n")
    f.write("\n// 根据字符获取位图指针和宽高\n")
    f.write("const unsigned char* getDigit(char c, int &w, int &h);\n")

# 写入 DigitData.cpp
with open(os.path.join(OUTPUT_DIR, "DigitData.cpp"), "w") as f:
    f.write('#include "DigitData.h"\n\n')
    for key in sorted(digit_data.keys()):
        d = digit_data[key]
        f.write(f"const unsigned char gDigit_{key}[{d['bytes']}] PROGMEM = {{\n")
        f.write(d["code"] + "\n};\n\n")
    # getDigit 函数
    f.write("const unsigned char* getDigit(char c, int &w, int &h) {\n")
    f.write("    w = DIGIT_W; h = DIGIT_H;\n")
    f.write("    switch (c) {\n")
    for i in range(10):
        f.write(f"        case '{i}': return gDigit_d_{i};\n")
    f.write("        case ':': return gDigit_d_colon;\n")
    f.write("        default: return gDigit_d_0;\n")
    f.write("    }\n}\n")

# ============================================================
# 3. 中文字模 (MAX 41×32, 32px高居中嵌入, 矩阵固定164字节)
# ============================================================
print("3. 生成中文字模 (32x32, 164B矩阵)...")

chars_text = (
    # 城市 + 天气
    "广州增城新塘北京晴多云阴雨阵雪雾霾雷"
    # 日期
    "年月日周一二三四五六"
    # 其他
    "湿度温度风力更新获取中加载失败无网络"
    # 符号
    "℃%"
)

# 去重 + 排序
chars_list = list(dict.fromkeys(chars_text))
print(f"   共 {len(chars_list)} 个汉字")

try:
    cn_font = ImageFont.truetype("/usr/share/fonts/opentype/noto/NotoSansCJK-Bold.ttc", 36)
except:
    try:
        cn_font = ImageFont.truetype("/usr/share/fonts/truetype/wqy/wqy-microhei.ttc", 36)
    except:
        cn_font = ImageFont.load_default()

# 字模规格 (匹配 fonts.h 中的 MAX_HEIGHT_FONT=41, MAX_WIDTH_FONT=32)
MATRIX_H = 41  # 矩阵总行数
MATRIX_W = 32  # 矩阵宽度
CH_H = 32      # 实际字符高度
CH_W = 32      # 实际字符宽度
ROW_BYTES = MATRIX_W // 8  # 4
MATRIX_SIZE = MATRIX_H * ROW_BYTES  # 164
TOP_PAD = (MATRIX_H - CH_H) // 2   # 顶部留白行数 (4行)
BOTTOM_PAD = MATRIX_H - CH_H - TOP_PAD  # 底部留白 (5行)

table_entries = []

for ch in chars_list:
    scale = 3
    img = Image.new("L", (CH_W * scale, CH_H * scale), 255)
    draw = ImageDraw.Draw(img)
    bbox = draw.textbbox((0, 0), ch, font=cn_font)
    tw = bbox[2] - bbox[0]
    th = bbox[3] - bbox[1]
    x = (CH_W * scale - tw) // 2 - bbox[0]
    y = (CH_H * scale - th) // 2 - bbox[1]
    draw.text((x, y), ch, font=cn_font, fill=0)
    img = img.resize((CH_W, CH_H), Image.LANCZOS)

    px = img.load()
    
    # 构建 164 字节矩阵 (41行 × 4字节/行)
    bitmap = []
    # 顶部留白
    for _ in range(TOP_PAD * ROW_BYTES):
        bitmap.append(0x00)
    # 实际字符数据 (32行)
    for row_y in range(CH_H):
        b = 0
        for bit in range(CH_W):
            if px[bit, row_y] <= 128:  # 深色 → 笔画 → bit=1
                b |= (1 << (7 - (bit % 8)))
            if bit % 8 == 7:
                bitmap.append(b)
                b = 0
    # 底部留白
    for _ in range(BOTTOM_PAD * ROW_BYTES):
        bitmap.append(0x00)

    assert len(bitmap) == MATRIX_SIZE, f"矩阵大小错误: {len(bitmap)} != {MATRIX_SIZE}"

    # UTF-8 编码 (3字节)
    utf8 = list(ch.encode('utf-8'))
    while len(utf8) < 3:
        utf8.append(0)

    table_entries.append({
        "ch": ch,
        "idx": f"0x{utf8[0]:02X}, 0x{utf8[1]:02X}, 0x{utf8[2]:02X}",
        "bitmap": bitmap
    })

# 写入 FontClockCN.h
with open(os.path.join(OUTPUT_DIR, "FontClockCN.h"), "w") as f:
    f.write("#pragma once\n#include \"fonts.h\"\n\nextern cFONT FontClockCN;\n")

# 写入 FontClockCN.cpp (匹配现有 CH_CN 格式: inline matrix[164])
with open(os.path.join(OUTPUT_DIR, "FontClockCN.cpp"), "w") as f:
    f.write('#include "FontClockCN.h"\n\n')

    f.write(f"static const CH_CN FontClockCN_Table[] = {{\n")
    for entry in table_entries:
        idx = entry["idx"]
        bm = entry["bitmap"]
        # 格式化 164 字节: 每行16个值
        bm_strs = [f"0x{b:02X}" for b in bm]
        bm_lines = []
        for i in range(0, len(bm_strs), 16):
            bm_lines.append("        " + ", ".join(bm_strs[i:i+16]) + ",")
        bm_formatted = "\n".join(bm_lines)
        f.write(f"  {{{{{idx}}}, {{\n{bm_formatted}\n  }}}},\n")
    f.write("};\n\n")

    f.write("cFONT FontClockCN = {\n")
    f.write(f"    FontClockCN_Table,\n")
    f.write(f"    {len(table_entries)},\n")
    f.write(f"    {CH_W // 2},  // ASCII_Width (half of CN)\n")
    f.write(f"    {CH_W},\n")
    f.write(f"    {CH_H}\n")
    f.write("};\n")

print(f"   生成 {len(table_entries)} 个字符")

# ============================================================
print("=" * 50)
print(f"全部资源已生成到: {OUTPUT_DIR}")
print("文件列表:")
for f in sorted(os.listdir(OUTPUT_DIR)):
    fpath = os.path.join(OUTPUT_DIR, f)
    if os.path.isfile(fpath):
        size = os.path.getsize(fpath)
        print(f"  {f} ({size:,} bytes)")
print("完成!")
