#!/bin/bash
set -e

echo "=== ESP32 开发环境一键部署脚本 ==="

# ============================================================
# 0. 代理配置（flclash / Clash）
# ============================================================
PROXY_HOST="127.0.0.1"
PROXY_HTTP_PORT="7890"      # Clash HTTP 代理端口，按实际情况修改
PROXY_SOCKS_PORT="7891"     # Clash SOCKS5 代理端口

HTTP_PROXY="http://${PROXY_HOST}:${PROXY_HTTP_PORT}"
HTTPS_PROXY="http://${PROXY_HOST}:${PROXY_HTTP_PORT}"
SOCKS_PROXY="socks5://${PROXY_HOST}:${PROXY_SOCKS_PORT}"

export HTTP_PROXY HTTPS_PROXY SOCKS_PROXY
export http_proxy="${HTTP_PROXY}"
export https_proxy="${HTTPS_PROXY}"
export socks_proxy="${SOCKS_PROXY}"

# 配置 Git 代理
git config --global http.proxy  "${HTTP_PROXY}"
git config --global https.proxy "${HTTPS_PROXY}"

# 配置 pip 代理
mkdir -p ~/.config/pip
cat > ~/.config/pip/pip.conf << PIPEOF
[global]
proxy = ${HTTP_PROXY}
PIPEOF

echo ">>> 代理已设置: HTTP=${HTTP_PROXY}  SOCKS5=${SOCKS_PROXY}"
echo ""

# 1. 系统依赖
sudo apt update
sudo apt install -y git wget flex bison gperf python3 python3-pip python3-venv \
    cmake ninja-build ccache libffi-dev libssl-dev dfu-util libusb-1.0-0 curl unzip xz-utils

# 2. 用户加入 dialout 组
sudo usermod -a -G dialout $USER

# 3. udev 规则
cat << 'EOF' | sudo tee /etc/udev/rules.d/99-esp32.rules
SUBSYSTEM=="usb", ATTR{idVendor}=="10c4", ATTR{idProduct}=="ea60", MODE="0666"
SUBSYSTEM=="usb", ATTR{idVendor}=="1a86", ATTR{idProduct}=="7523", MODE="0666"
SUBSYSTEM=="usb", ATTR{idVendor}=="0403", ATTR{idProduct}=="6001", MODE="0666"
SUBSYSTEM=="usb", ATTR{idVendor}=="303a", ATTR{idProduct}=="1001", MODE="0666"
EOF
sudo udevadm control --reload-rules
sudo udevadm trigger

# 4. 安装 VS Code（如果未安装）
if ! command -v code &> /dev/null; then
    sudo snap install --classic code
fi

# 5. 克隆 ESP-IDF（针对大仓库/代理环境优化）
mkdir -p ~/esp
cd ~/esp
if [ ! -d "esp-idf" ]; then
    # 增大 Git 缓冲区，避免大仓库代理环境下 RPC 失败
    git config --global http.postBuffer 524288000
    git config --global http.lowSpeedLimit 0
    git config --global http.lowSpeedTime 999999

    echo ">>> 开始克隆 ESP-IDF（采用浅克隆以减少下载量）..."
    git clone --depth 1 --single-branch -b v5.4 \
        https://github.com/espressif/esp-idf.git

    echo ">>> 拉取子模块..."
    cd esp-idf
    git submodule update --init --recursive --depth 1
    cd ..
fi

# 6. 安装工具链
cd ~/esp/esp-idf
./install.sh esp32

# 7. 安装 VS Code 扩展
code --install-extension espressif.esp-idf-extension

# 8. 设置别名
grep -q "get_idf" ~/.bashrc || echo "alias get_idf='. ~/esp/esp-idf/export.sh'" >> ~/.bashrc

echo ""
echo "=== 部署完成！ ==="
echo "请执行以下操作："
echo "1. 注销并重新登录（使 dialout 组生效）"
echo "2. 打开 VS Code"
echo "3. Ctrl+Shift+P → 'ESP-IDF: Configure ESP-IDF Extension'"
echo "4. 选择 'Use Existing Setup'，IDF 路径填 ~/esp/esp-idf"
echo ""
echo "--- 代理提示 ---"
echo "本脚本已配置 Git 全局代理: ${HTTP_PROXY}"
echo "如需取消 Git 代理，请执行:"
echo "  git config --global --unset http.proxy"
echo "  git config --global --unset https.proxy"
echo "如需取消 pip 代理，删除 ~/.config/pip/pip.conf 中的 proxy 行即可"
echo "========================================="
