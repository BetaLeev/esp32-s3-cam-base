# ESP32 项目 Bug 修复工作规范

## 1. 目的与范围

本规范定义了 ESP32-S3 项目中 bug 修复的标准流程、质量要求和技术规范，确保修复工作安全、可追溯、高质量。

**适用范围**：本项目所有 bug 修复任务

---

## 2. 问题分析阶段

### 2.1 必做事项

| 序号 | 任务 | 说明 |
|------|------|------|
| 1 | 复现问题 | 在目标环境中实际触发并观察 bug |
| 2 | 收集日志 | 串口日志、网络抓包、控制台输出 |
| 3 | 定位代码 | 找到问题相关的源文件（.c/.h/.js） |
| 4 | 分析根因 | 确定问题的根本原因（不仅是表象） |
| 5 | 评估影响 | 评估修复对其他功能的影响范围 |

### 2.2 代码定位参考

| 功能模块 | 后端代码 | 前端代码 |
|----------|----------|----------|
| SD卡文件管理 | `src/sdcard/` | `data/web/sdcard.js` |
| HTTP服务器 | `src/http_server.c` | - |
| 水泵/舵机控制 | `src/actuators/` | `data/web/app.js` |
| 传感器数据 | `src/sensors/` | `data/web/app.js` |
| WiFi连接 | `src/wifi_app.c` | - |

### 2.3 根因分析模板

```markdown
**问题描述**：[简述问题]

**问题定位**：
- 文件：`src/xxx.c` 或 `data/web/xxx.js`
- 函数/方法：
- 行号：

**根因分析**：
[详细说明为什么会发生这个问题]

**证据**：
- 日志输出：
- 调试信息：
```

---

## 3. 修复方案设计

### 3.1 风险等级划分

| 等级 | 定义 | 修复要求 |
|------|------|----------|
| **低风险** | 仅修改前端展示层逻辑，不涉及核心数据处理 | 可直接修复 |
| **中风险** | 修改后端业务逻辑，可能影响数据一致性 | 需要代码审查 |
| **高风险** | 修改配置文件、底层驱动、内存管理 | 需要完整测试 |

### 3.2 修复方案模板

```markdown
## 修复方案

**修改文件**：`xxx.c` 或 `xxx.js`

**修改位置**：
- 文件路径和行号
- 具体函数名

**修复前代码**：
```c
// 或 javascript
```

**修复后代码**：
```c
// 或 javascript
```

**修改原因**：
[解释为什么要这样修改]
```

### 3.3 影响评估

```markdown
**影响范围**：
- 直接影响：
- 间接影响：

**依赖关系**：
```
模块A
  ↓
模块B（本次修复）
  ↓
模块C
```

**需同步修改**：
- [ ] 依赖模块X
- [ ] 配置文件Y
```

---

## 4. 修复实施

### 4.1 修复优先级

1. **前端修复优先** → 尽量在表现层解决问题
2. **最小改动原则** → 只改必要的代码
3. **向后兼容** → 不破坏现有功能
4. **统一编码规范** → 字符集、换行符等保持一致

### 4.2 本项目字符编码规范

| 类型 | 规范 | 说明 |
|------|------|------|
| C源文件 | UTF-8 无 BOM | 标准编码 |
| JSON响应 | `application/json; charset=utf-8` | 必须指定字符集 |
| HTML响应 | `text/html; charset=utf-8` | 必须指定字符集 |
| JavaScript | UTF-8 | 文件本身编码 |
| FatFS配置 | Code Page 936 + UTF-8 | 支持中文文件名 |

### 4.3 URL处理规范

| 场景 | 处理方式 |
|------|----------|
| 后端生成URL | 使用 `url_encode()` 编码后返回前端 |
| 前端构造URL | 传递未编码数据，统一在发送时编码 |
| 前端获取URL | 后端返回已编码URL，前端不再重复编码 |

**错误示例**：
```javascript
// 双重编码
const src = '/fs/files?path=' + encodeURIComponent(encodedPath);  // ❌
```

**正确示例**：
```javascript
// 单次编码
const src = '/fs/files?path=' + encodeURIComponent(rawPath);     // ✓
```

### 4.4 内存管理规范

| 场景 | 规范 |
|------|------|
| 动态分配 | 优先使用 `malloc()`，完成后必须 `free()` |
| 错误处理 | 分配失败时释放已分配资源 |
| 缓冲区大小 | 根据实际数据大小合理设置，避免栈溢出 |
| 字符串操作 | 使用 `snprintf()` 而非 `sprintf()` |

---

## 5. 验证测试

### 5.1 编译验证

```bash
# 清理编译
pio run -e esp32-s3-cam -t clean

# 重新编译
pio run -e esp32-s3-cam

# 观察编译输出，确保无警告无错误
```

### 5.2 烧录流程

```bash
# 烧录固件
pio run -e esp32-s3-cam -t upload

# 烧录文件系统（如果修改了 web/ 目录）
pio run -e esp32-s3-cam -t uploadfs
```

### 5.3 功能测试清单

| 测试项 | 测试方法 | 预期结果 |
|--------|----------|----------|
| 问题复现测试 | 重复触发bug的条件 | bug不再出现 |
| 边界条件测试 | 极端输入、空值、超长字符串 | 正常处理或明确报错 |
| 回归测试 | 原有功能不受影响 | 所有功能正常 |
| 日志检查 | 串口输出正常 | 无异常错误信息 |

### 5.4 测试记录模板

```markdown
## 测试记录

**测试时间**：
**测试人员**：
**环境版本**：

| 测试项 | 操作步骤 | 预期结果 | 实际结果 | 通过 |
|--------|----------|----------|----------|------|
| 复现测试 | ... | ... | ... | ✓/✗ |
| 功能A | ... | ... | ... | ✓/✗ |
| 功能B | ... | ... | ... | ✓/✗ |
```

---

## 6. 回滚方案

### 6.1 回滚条件

| 条件 | 描述 |
|------|------|
| 编译失败 | 修改后无法通过编译 |
| 功能异常 | 修复导致其他功能故障 |
| 性能下降 | 修复引入明显性能问题 |
| 内存问题 | 导致内存泄漏或溢出 |

### 6.2 回滚步骤

```bash
# 1. 使用 Git 恢复文件
git checkout -- src/xxx.c

# 2. 重新编译烧录验证
pio run -e esp32-s3-cam -t clean
pio run -e esp32-s3-cam -t upload
```

### 6.3 配置修改回滚

**sdkconfig 回滚**：
```bash
# 查看修改
git diff sdkconfig.esp32-s3-cam

# 恢复特定行
git checkout -p sdkconfig.esp32-s3-cam
```

---

## 7. 代码审查清单

### 7.1 修复前检查

- [ ] 问题已准确复现
- [ ] 根因已明确
- [ ] 修复方案已设计
- [ ] 影响范围已评估

### 7.2 修复后检查

- [ ] 编译通过无警告
- [ ] 问题已修复
- [ ] 相关功能正常
- [ ] 日志无异常
- [ ] 资源正确释放（内存、文件句柄）

### 7.3 代码质量检查

- [ ] 命名规范（变量、函数）
- [ ] 注释清晰（关键逻辑）
- [ ] 缩进一致
- [ ] 无调试代码残留
- [ ] 错误处理完整

---

## 8. 文档更新

修复完成后，根据需要更新以下文档：

| 文档 | 更新内容 |
|------|----------|
| 代码注释 | 复杂逻辑说明 |
| CHANGELOG | 修复内容记录 |
| API文档 | 接口变更说明 |

---

## 9. 本次SD卡模块修复详情

### 9.1 问题汇总

| # | 问题描述 | 根因 | 风险等级 |
|---|----------|------|----------|
| 1 | 音频/视频加载失败 | 前端双重URL编码 | 低 |
| 2 | 中文文件名显示下划线 | FatFS Code Page 437 | 高 |
| 3 | 中文文件名显示下划线 | JSON无UTF-8标识 | 中 |

### 9.2 修复清单

#### 修复1：sdcard.js（低风险）

**文件**：`data/web/sdcard.js`

| 行号 | 函数 | 修改内容 |
|------|------|----------|
| 246 | `openImagePreview` | `path` → `fullPath` |
| 264 | `playAudio` | `path` → `fullPath` |
| 299 | `playVideo` | `path` → `fullPath` |

#### 修复2：sdkconfig（高风险）

**文件**：`sdkconfig.esp32-s3-cam`

```yaml
# 删除 Code Page 437
# CONFIG_FATFS_CODEPAGE_437=y

# 添加 Code Page 936（简体中文）
CONFIG_FATFS_CODEPAGE_936=y
CONFIG_FATFS_CODEPAGE=936

# 改用 UTF-8 API 编码
# CONFIG_FATFS_API_ENCODING_ANSI_OEM is not set
CONFIG_FATFS_API_ENCODING_UTF_8=y
```

#### 修复3：sdcard_web.c（中风险）

**文件**：`src/sdcard/sdcard_web.c`

| 行号 | 函数 | 修改内容 |
|------|------|----------|
| 334 | `sdcard_web_files_handler` | 添加 `; charset=utf-8` |
| 414 | `sdcard_web_info_handler` | 添加 `; charset=utf-8` |
| 803 | `sdcard_web_upload_handler` | 添加 `; charset=utf-8` |

### 9.3 修改顺序

```
1. sdkconfig（依赖底层）
   ↓
2. sdcard_web.c（依赖sdkconfig）
   ↓
3. sdcard.js（依赖后端API）
```

### 9.4 验证测试

**编译测试**：
```bash
pio run -e esp32-s3-cam -t clean
pio run -e esp32-s3-cam
```

**功能测试**：
1. 上传带中文名称的音频文件（如 `歌曲.mp3`）
2. 确认文件列表正确显示中文名
3. 点击播放，确认加载成功并正常播放

---

## 10. 附录

### A. Git 使用规范

```bash
# 创建修复分支（可选）
git checkout -b fix/sdcard-encoding

# 查看修改
git diff

# 提交修改
git add src/sdcard/sdcard_web.c data/web/sdcard.js
git commit -m "fix: 修复SD卡中文文件名显示和音视频加载问题"
```

### B. 串口日志查看

```bash
# 使用 PlatformIO 查看日志
pio device monitor -b 115200

# 筛选特定TAG
pio device monitor --filter "esp32"
```

### C. 相关文件路径

```
/Users/v/Desktop/esp32_demo/
├── src/
│   ├── sdcard/
│   │   ├── sdcard.c          # SD卡底层驱动
│   │   └── sdcard_web.c      # SD卡HTTP处理
│   ├── http_server.c         # HTTP服务器
│   └── main.c                # 主程序
├── data/web/
│   ├── sdcard.js             # SD卡前端逻辑
│   └── index.html            # 主页
├── sdkconfig.esp32-s3-cam     # ESP-IDF配置
└── SDCARD_FIX_RULES.md       # 本规范文件
```
