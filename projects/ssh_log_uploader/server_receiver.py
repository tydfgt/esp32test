#!/usr/bin/env python3
"""
ESP32 日志接收服务器（HTTP 备选方案）
运行在远程 Linux 服务器上，接收 ESP32 发来的日志并写入文件。

启动方式：
    python3 server_receiver.py

默认监听 8080 端口，日志保存到指定路径。
注意：本项目已改用 SSH exec 方案，此脚本仅作备选参考。
"""

import os
from http.server import HTTPServer, BaseHTTPRequestHandler
from datetime import datetime

LOG_DIR = "/root/esp32"
LOG_FILE = os.path.join(LOG_DIR, "esp32_log.txt")
LISTEN_PORT = 8080


class LogHandler(BaseHTTPRequestHandler):
    """接收 ESP32 POST 的日志数据，追加写入日志文件"""

    def do_POST(self):
        content_length = int(self.headers.get("Content-Length", 0))
        body = self.rfile.read(content_length).decode("utf-8")

        # 确保日志目录存在
        os.makedirs(LOG_DIR, exist_ok=True)

        # 追加写入日志
        timestamp = datetime.now().strftime("[%Y-%m-%d %H:%M:%S]")
        with open(LOG_FILE, "a", encoding="utf-8") as f:
            f.write(f"{timestamp} {body}\n")

        print(f"[收到] {body}")

        self.send_response(200)
        self.send_header("Content-Type", "text/plain")
        self.end_headers()
        self.wfile.write(b"OK")

    def log_message(self, format, *args):
        """自定义日志格式"""
        print(f"[{datetime.now().strftime('%H:%M:%S')}] {args[0]}")


if __name__ == "__main__":
    os.makedirs(LOG_DIR, exist_ok=True)
    print(f"=== ESP32 日志接收服务器 ===")
    print(f"监听端口: {LISTEN_PORT}")
    print(f"日志文件: {LOG_FILE}")
    print(f"按 Ctrl+C 停止\n")

    server = HTTPServer(("0.0.0.0", LISTEN_PORT), LogHandler)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\n服务器已停止")
        server.server_close()
