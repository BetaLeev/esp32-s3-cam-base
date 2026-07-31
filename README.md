# ESP32-S3 智能水泵控制系统

基于ESP32-S3-CAM开发板的智能水泵控制与环境监测系统，支持Wi-Fi热点和Web控制界面。

## 功能特性

- **Wi-Fi AP模式**：作为热点提供网络接入
- **温湿度监测**：DHT11传感器实时采集环境数据
- **水泵控制**：支持开关和PWM调速（0-100%）
- **Web控制界面**：响应式设计，支持手机/电脑访问

## 硬件配置

| 模块 | 引脚 | 说明 |
|------|------|------|
| PWMA | GPIO 1 | LEDC PWM输出 |
| AIN1 | GPIO 2 | 电机方向控制 |
| AIN2 | GPIO 42 | 电机方向控制 |
| DHT11 | GPIO 41 | 温湿度传感器 |

**驱动模块**：TB6612（FNG双驱动）

## 软件架构

```
src/
├── main.c           # 主入口
├── config.h         # 全局配置
├── wifi_app.[hc]    # Wi-Fi AP模式
├── dht11.[hc]       # DHT11传感器
├── motor_ctrl.[hc]   # TB6612电机驱动
├── http_server.[hc]  # HTTP服务器
└── web_module.[hc]   # Web静态资源
```

## 快速开始

### 1. 编译

```bash
pio run
```

### 2. 烧录

```bash
pio run --target upload
```

### 3. 使用

1. 连接Wi-Fi热点：`xiangjiazhgebu`（密码：`bjbjbjbj`）
2. 打开浏览器访问：`http://192.168.4.1`
3. 控制水泵开关和流速

## API接口

| 接口 | 说明 |
|------|------|
| `GET /` | Web控制页面 |
| `GET /api/status` | 获取系统状态 |
| `GET /api/pump?action=on/off&speed=50` | 控制水泵 |
| `GET /api/config` | 获取配置信息 |
| `GET /styles.css` | 样式文件 |
| `GET /app.js` | JavaScript文件 |

## 目录说明

- `src/` - 源代码
- `src/web/` - Web前端源文件（HTML/CSS/JS）
- `.pio/` - PlatformIO构建目录
