# ESP32 项目基础规范

## 项目结构

```
esp32_demo/                    # 后端/固件项目 (ESP-IDF)
├── src/                      # C 源代码
│   ├── CMakeLists.txt        # ⚠️ 组件依赖在 platformio.ini
│   ├── main.c               # 主程序入口
│   ├── http_server.c/h      # HTTP 服务器
│   ├── wifi_app.c/h         # WiFi 应用
│   ├── wifi_config.c/h      # WiFi 配置 (NVS)
│   ├── wifi_web.c/h         # WiFi Web API
│   ├── sensors/             # 传感器模块
│   ├── actuators/           # 执行器模块
│   ├── sdcard/             # TF 卡模块
│   └── config/             # 配置模块
├── data/web/                # SPIFFS 前端静态文件
└── platformio.ini            # ⚠️ PlatformIO 配置

esp32-webui/                  # 前端项目 (Vue3)
├── src/
│   ├── views/               # 页面模块 (每个页面一个目录)
│   ├── components/         # 全局组件 (驼峰)
│   ├── api/                # API 调用封装
│   └── router/             # 路由配置
├── .env.development         # ⚠️ 开发环境 API 地址
└── .env.production          # 生产环境 (烧录用)
```

## 基本信息

| 项目 | 技术栈 | 芯片 |
|------|--------|------|
| 后端 | ESP-IDF, C, PlatformIO | ESP32-S3 |
| 前端 | Vue3, Element Plus, Vite | - |

## ⚠️ 重要提醒

### 后端开发

1. **新增 `.c/.h` 文件**
   - 必须检查 `platformio.ini` 的 `board_build.esp-idf.components`
   - 参见 `.rules/esp32-component-dependency.md`

2. **模块化开发**
   - 功能代码：`xxx.c/h`
   - Web API：`xxx_web.c/h`
   - 详见 `.rules/project-naming.md`

### 前端开发

1. **环境变量**
   - `.env.development` - 开发时连接的后端 IP
   - `VITE_API_BASE_URL=http://192.168.x.x`

2. **模块化开发**
   - 页面模块：`views/xxx/index.vue`
   - 全局组件：`components/Xxx.vue` (驼峰)
   - 详见 `.rules/project-naming.md`

3. **路由模式**
   - 必须使用 Hash 模式 (`createWebHashHistory`)

## API 接口

| 接口 | 说明 |
|------|------|
| `/api/status` | 系统状态 (传感器+执行器) |
| `/api/pump` | 水泵控制 |
| `/api/servo` | 舵机控制 |
| `/api/sdcard/*` | TF 卡管理 |
| `/api/wifi/*` | WiFi 配置 |

## 调试检查清单

- [ ] 修改后端代码后重新编译烧录
- [ ] 修改前端后 `npm run dev` 确认效果
- [ ] 生产部署前 `npm run build`
