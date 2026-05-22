#!/bin/bash
# 一键编译+烧录 5.83寸墨水屏项目
# 用法: bash upload.sh [项目目录]
#       默认使用当前目录

set -e

PROJECT_DIR="${1:-.}"
PORT="/dev/ttyACM0"
FQBN="esp32:esp32:esp32"

export PATH="$HOME/.local/bin:$PATH"

cd "$PROJECT_DIR"

echo "========================================="
echo "  墨水屏项目编译 & 烧录"
echo "  项目: $(pwd)"
echo "  端口: $PORT"
echo "  板卡: $FQBN"
echo "========================================="

echo ""
echo "[1/2] 编译中..."
arduino-cli compile --fqbn "$FQBN"

echo ""
echo "[2/2] 烧录中..."
arduino-cli upload --fqbn "$FQBN" -p "$PORT"

echo ""
echo "========================================="
echo "  完成！墨水屏应已更新"
echo "========================================="
