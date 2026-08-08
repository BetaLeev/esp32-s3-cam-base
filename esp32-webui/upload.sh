#!/bin/bash

# ESP32 SPIFFS 上传脚本
# 使用方式: ./upload.sh <串口端口>

SERIAL_PORT=${1:-"/dev/ttyUSB0"}
PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"
SPIFFS_DIR="$PROJECT_DIR/dist"

echo "=========================================="
echo "ESP32 SPIFFS 上传工具"
echo "=========================================="
echo "串口端口: $SERIAL_PORT"
echo "SPIFFS目录: $SPIFFS_DIR"
echo ""

# 检查 Python 环境
if ! command -v python3 &> /dev/null; then
    echo "错误: 需要安装 Python3"
    exit 1
fi

# 检查 esptool
if ! python3 -m esptool --version &> /dev/null; then
    echo "正在安装 esptool..."
    pip3 install esptool
fi

# 检查串口
if [ ! -e "$SERIAL_PORT" ]; then
    echo "错误: 串口 $SERIAL_PORT 不存在"
    echo "可用串口:"
    ls -l /dev/tty.* 2>/dev/null || ls -l /dev/cu.* 2>/dev/null
    exit 1
fi

echo "开始上传 SPIFFS 文件..."
python3 -m esptool --chip esp32 \
    --port "$SERIAL_PORT" \
    --baud 921600 \
    before_default_reset \
    erase_flash \
    run \
    no_reset

# 常用命令参考
echo ""
echo "=========================================="
echo "常用烧录命令:"
echo "=========================================="
echo ""
echo "# 查看可用串口"
echo "ls /dev/tty.*"
echo ""
echo "# 擦除闪存"
echo "esptool.py --chip esp32 --port $SERIAL_PORT erase_flash"
echo ""
echo "# 烧录 SPIFFS (示例，根据实际情况调整地址)"
echo "mkspiffs -c dist -b 4096 -p 256 -s 0x350000 spiffs.bin"
echo "esptool.py --chip esp32 --port $SERIAL_PORT write_flash 0x350000 spiffs.bin"
echo ""
echo "# 一键烧录 (需要 esp32fs 插件)"
echo "npm run upload-spiffs"
echo ""
