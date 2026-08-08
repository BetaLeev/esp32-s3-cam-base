# 项目模块命名规范

## 前端项目 (esp32-webui/)

### 目录结构规范

```
src/
├── api/                    # API 模块
│   ├── index.js          # 统一入口
│   └── esp32.js         # 具体 API
├── components/            # 全局组件 (驼峰)
│   ├── FileList.vue      # 驼峰命名
│   └── ImagePreview.vue
├── views/                 # 页面模块 (每个页面一个目录)
│   ├── control/          # 设备控制模块
│   │   ├── index.vue    # 统一入口
│   │   └── status-card.vue
│   ├── files/            # 文件管理模块
│   │   ├── index.vue
│   │   ├── preview.vue
│   │   └── player.vue
│   └── wifi/            # WiFi 配置模块
│       ├── index.vue
│       └── network-list.vue
└── router/
    └── index.js
```

### 命名规则

| 类型 | 规则 | 示例 |
|------|------|------|
| 全局组件 | 驼峰 | `FileList.vue`, `ImagePreview.vue` |
| 模块目录 | 英文小写 | `control/`, `files/`, `wifi/` |
| 页面入口 | `index.vue` | `control/index.vue` |
| 页面子组件 | 英文小写+连字符 | `status-card.vue` |
| API 模块 | 英文小写 | `esp32.js` |

### 错误示例 ❌

```
src/
├── views/
│   ├── ControlView.vue      # ❌ 应该用目录
│   ├── FilesView.vue        # ❌ 应该用目录
│   └── file-manager.js     # ❌ 目录内不用连字符
```

### 正确示例 ✅

```
src/
├── views/
│   ├── control/
│   │   ├── index.vue
│   │   └── status-card.vue
│   └── files/
│       ├── index.vue
│       └── preview.vue
```

---

## 后端项目 (esp32_demo/)

### 目录结构规范

```
src/
├── http_server.c/h        # HTTP 服务器 (主入口)
├── wifi_app.c/h          # WiFi 应用
├── wifi_config.c/h      # WiFi 配置 (NVS)
├── wifi_web.c/h         # WiFi Web API
├── sensors/             # 传感器模块
│   ├── sensors.c/h
│   └── sensors_web.c/h
├── actuators/          # 执行器模块
│   ├── actuators.c/h
│   └── actuators_web.c/h
├── sdcard/             # TF 卡模块
│   ├── sdcard.c/h
│   └── sdcard_web.c/h
└── config/             # 配置模块
    ├── config.h
    └── hw_xxx.h
```

### 命名规则

| 类型 | 规则 | 示例 |
|------|------|------|
| 功能模块 | `xxx.c/h` | `wifi_config.c/h` |
| Web API | `xxx_web.c/h` | `wifi_web.c/h` |
| 子模块目录 | 英文小写 | `sensors/`, `actuators/` |

### 错误示例 ❌

```
src/
├── wifiFunctions.c      # ❌ 没有分类
├── webAPI.c             # ❌ 命名不统一
└── Sensor.c             # ❌ 大写开头
```

### 正确示例 ✅

```
src/
├── wifi_config.c/h      # WiFi 配置
├── wifi_web.c/h         # WiFi API
└── sensors/
    ├── sensors.c/h
    └── sensors_web.c/h
```

---

## 新增模块检查清单

### 前端新增页面
- [ ] 在 `views/` 下创建目录？
- [ ] 入口文件命名为 `index.vue`？
- [ ] 子组件用连字符小写？

### 后端新增模块
- [ ] 创建独立目录？
- [ ] 分离功能代码和 Web API？
- [ ] 功能代码：`xxx.c/h`
- [ ] Web API：`xxx_web.c/h`
