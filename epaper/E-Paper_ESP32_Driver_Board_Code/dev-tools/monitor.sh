#!/bin/bash
# 串口监视器
# 用法: bash monitor.sh

PORT="/dev/ttyACM0"
BAUD="115200"

export PATH="$HOME/.local/bin:$PATH"

echo "连接墨水屏驱动板串口..."
echo "端口: $PORT  波特率: $BAUD"
echo "按 Ctrl+C 退出"
echo ""

arduino-cli monitor -p "$PORT" -c baudrate="$BAUD"
