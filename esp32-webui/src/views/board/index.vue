<template>
  <div class="board-view">
    <!-- 第一行：图片 + 板子信息 -->
    <el-row :gutter="20" class="top-row">
      <el-col :xs="24" :sm="24" :md="8">
        <PageCard title="板子实物图" icon="Picture">
          <div class="image-container">
            <el-image
              v-if="imageUrl"
              :src="imageUrl"
              :zoom-rate="1.2"
              :preview-src-list="[imageUrl]"
              fit="contain"
              class="board-image"
            >
              <template #error>
                <EmptyState type="error" text="图片加载失败" />
              </template>
            </el-image>
            <EmptyState v-else type="no-data" text="暂无图片" />
          </div>
          <div class="image-hint">
            <el-icon><ZoomIn /></el-icon>
            <span>点击图片可放大预览</span>
          </div>
        </PageCard>
      </el-col>

      <el-col :xs="24" :sm="24" :md="16">
        <BoardInfoCard :board-info="boardInfo" />
        <BoardInfoActions @reboot="onReboot" @sleep="onSleep" />
      </el-col>
    </el-row>

    <!-- 第二行：温度监控（只显示芯片温度） -->
    <PageCard
      title="芯片温度监控"
      icon="HotWater"
      :refreshable="true"
      :loading="tempLoading"
      @refresh="fetchTempData"
    >
      <div class="temp-display">
        <div class="temp-gauge">
          <div class="temp-circle" :class="tempClass">
            <div class="temp-inner">
              <div class="temp-value">{{ displayTemp }}</div>
              <div class="temp-unit">°C</div>
            </div>
          </div>
          <div class="temp-label">芯片内部温度</div>
        </div>

        <div class="temp-info">
          <el-descriptions :column="1" border size="small">
            <el-descriptions-item label="传感器状态">
              <el-tag v-if="tempData.sensor_ok" type="success" size="small">正常</el-tag>
              <el-tag v-else type="danger" size="small">未初始化</el-tag>
            </el-descriptions-item>
            <el-descriptions-item label="温度状态">
              <el-tag v-if="tempStatus" :type="tempStatus.tagType" size="small">
                {{ tempStatus.message }}
              </el-tag>
            </el-descriptions-item>
            <el-descriptions-item label="说明">
              <span class="text-muted">ESP32-S3 内置温度传感器，测量芯片内部温度</span>
            </el-descriptions-item>
          </el-descriptions>
        </div>
      </div>
    </PageCard>

    <!-- 第三行：系统资源 -->
    <PageCard title="系统资源" icon="Monitor">
      <el-descriptions :column="2" border size="small">
        <el-descriptions-item label="DRAM">
          {{ formatBytes(status.dram_free) }} / {{ formatBytes(status.dram_total) }}
        </el-descriptions-item>
        <el-descriptions-item label="PSRAM">
          {{ formatBytes(status.psram_free) }} / {{ formatBytes(status.psram_total) }}
        </el-descriptions-item>
        <el-descriptions-item label="Flash">
          {{ formatBytes(status.flash_total) }}
        </el-descriptions-item>
        <el-descriptions-item label="SPIFFS">
          {{ formatBytes(status.spiffs_free) }} / {{ formatBytes(status.spiffs_total) }}
        </el-descriptions-item>
        <el-descriptions-item label="TF卡">
          <span :class="status.sdcard_mounted ? 'text-success' : 'text-muted'">
            {{ status.sdcard_mounted ? formatBytes(status.sdcard_free) + ' 可用' : '未挂载' }}
          </span>
        </el-descriptions-item>
        <el-descriptions-item label="运行时间">
          {{ formatUptime(status.uptime_seconds) }}
        </el-descriptions-item>
      </el-descriptions>
    </PageCard>

    <!-- 第四行：管脚列表 -->
    <PinList :pins="pins" />
  </div>
</template>

<script setup>
import { ref, reactive, computed, onMounted, onUnmounted } from 'vue'
import { Picture, Monitor, ZoomIn } from '@element-plus/icons-vue'
import { PageCard, EmptyState } from '@/components/common'
import { BoardInfoCard, BoardInfoActions, PinList } from './components'
import { getBoardInfoSafe, getBoardTempSafe, getStatusSafe } from '@/api/esp32'

// 图片路径
const imageUrl = new URL('@/assets/images/esp32-s3-cam.png', import.meta.url).href

// 板子基本信息
const boardInfo = reactive({
  chip_model: '',
  firmware_version: '',
  board_name: '',
  build_time: '',
  uptime: '',
  uptime_seconds: 0,
  free_heap: ''
})

// 温度数据（只保留 chip_temp）
const tempData = reactive({
  chip_temp: 0,
  sensor_ok: false
})

// 系统状态数据
const systemStatus = reactive({
  dram_free: 0,
  dram_total: 0,
  psram_free: 0,
  psram_total: 0,
  flash_total: 0,
  spiffs_free: 0,
  spiffs_total: 0,
  sdcard_mounted: false,
  sdcard_free: 0,
  uptime_seconds: 0
})

// 合并 status 引用
const status = systemStatus

const tempLoading = ref(false)

// 显示温度值
const displayTemp = computed(() => {
  const temp = parseFloat(tempData.chip_temp) || 0
  return temp.toFixed(1)
})

// 温度样式类
const tempClass = computed(() => {
  const temp = parseFloat(tempData.chip_temp) || 0
  if (temp >= 70) return 'temp-high'
  if (temp >= 50) return 'temp-warm'
  if (temp > 0) return 'temp-normal'
  return 'temp-unknown'
})

// 温度状态提示
const tempStatus = computed(() => {
  const temp = parseFloat(tempData.chip_temp) || 0
  if (!tempData.sensor_ok) {
    return { tagType: 'info', message: '传感器未初始化' }
  }
  if (temp >= 80) {
    return { tagType: 'danger', message: '温度过高，请检查散热' }
  } else if (temp >= 60) {
    return { tagType: 'warning', message: '温度偏高，建议加强散热' }
  } else if (temp >= 40) {
    return { tagType: 'success', message: '温度正常' }
  } else if (temp > 0) {
    return { tagType: 'success', message: '温度正常' }
  }
  return { tagType: 'info', message: '等待数据...' }
})

// ========================================
// 数据获取（使用安全API，后端不可用时保持默认值）
// ========================================
const fetchBoardInfo = async () => {
  const { data: resData } = await getBoardInfoSafe()
  if (resData) {
    boardInfo.chip_model = resData.chip_model || ''
    boardInfo.firmware_version = resData.firmware_version || ''
    boardInfo.board_name = resData.board_name || ''
    boardInfo.build_time = resData.build_time || ''
    boardInfo.uptime = resData.uptime || ''
    boardInfo.uptime_seconds = resData.uptime_seconds || 0
    boardInfo.free_heap = resData.free_heap || ''
  }
}

const fetchTempData = async () => {
  tempLoading.value = true
  const { data: resData } = await getBoardTempSafe()
  if (resData) {
    // ESP32-S3 只有内置温度传感器，只读取 chip_temp
    tempData.chip_temp = resData.chip_temp || 0
    tempData.sensor_ok = resData.sensor_ok || false
  }
  tempLoading.value = false
}

const fetchSystemStatus = async () => {
  const { data: resData } = await getStatusSafe()
  if (resData) {
    systemStatus.dram_free = resData.dram_free || 0
    systemStatus.dram_total = resData.dram_total || 0
    systemStatus.psram_free = resData.psram_free || 0
    systemStatus.psram_total = resData.psram_total || 0
    systemStatus.flash_total = resData.flash_total || 0
    systemStatus.spiffs_free = resData.spiffs_free || 0
    systemStatus.spiffs_total = resData.spiffs_total || 0
    systemStatus.sdcard_mounted = resData.sdcard_mounted || false
    systemStatus.sdcard_free = resData.sdcard_free || 0
    systemStatus.uptime_seconds = resData.uptime_seconds || 0
  }
}

// ========================================
// 格式化函数
// ========================================
const formatBytes = (bytes) => {
  if (!bytes) return '--'
  bytes = Number(bytes)
  if (bytes >= 1024 * 1024 * 1024) return (bytes / (1024 * 1024 * 1024)).toFixed(2) + ' GB'
  if (bytes >= 1024 * 1024) return (bytes / (1024 * 1024)).toFixed(2) + ' MB'
  if (bytes >= 1024) return (bytes / 1024).toFixed(2) + ' KB'
  return bytes + ' B'
}

const formatUptime = (seconds) => {
  if (!seconds) return '--'
  const h = Math.floor(seconds / 3600)
  const m = Math.floor((seconds % 3600) / 60)
  const s = seconds % 60
  return `${h}小时${m}分${s}秒`
}

// ========================================
// 事件处理
// ========================================
const onReboot = () => console.log('Reboot triggered')
const onSleep = () => console.log('Sleep triggered')

// ========================================
// 管脚数据 (静态数据)
// ========================================
const pins = [
  // 核心/系统占用管脚
  { gpio: 26, module: 'SPI Flash', function: 'SPI0/1 Flash', type: 'Core', status: 'reserved', is_core: true, warning: '系统核心管脚，不可作为GPIO使用', description: '片内Flash接口，系统启动必需' },
  { gpio: 27, module: 'SPI Flash', function: 'SPI0/1 Flash', type: 'Core', status: 'reserved', is_core: true, warning: '系统核心管脚', description: '片内Flash接口' },
  { gpio: 28, module: 'SPI Flash', function: 'SPI0/1 Flash', type: 'Core', status: 'reserved', is_core: true, description: '片内Flash接口' },
  { gpio: 29, module: 'SPI Flash', function: 'SPI0/1 Flash', type: 'Core', status: 'reserved', is_core: true, description: '片内Flash接口' },
  { gpio: 30, module: 'PSRAM', function: 'Octal PSRAM DQ0', type: 'Core', status: 'reserved', is_core: true, description: 'PSRAM数据线0' },
  { gpio: 31, module: 'PSRAM', function: 'Octal PSRAM DQ1', type: 'Core', status: 'reserved', is_core: true, description: 'PSRAM数据线1' },
  { gpio: 32, module: 'PSRAM', function: 'Octal PSRAM DQ2', type: 'Core', status: 'reserved', is_core: true, description: 'PSRAM数据线2' },
  { gpio: 33, module: 'PSRAM', function: 'Octal PSRAM DQ3', type: 'Core', status: 'reserved', is_core: true, description: 'PSRAM数据线3' },
  { gpio: 34, module: 'PSRAM', function: 'Octal PSRAM DQ4', type: 'Core', status: 'reserved', is_core: true, description: 'PSRAM数据线4' },
  { gpio: 35, module: 'PSRAM', function: 'Octal PSRAM DQ5', type: 'Core', status: 'reserved', is_core: true, description: 'PSRAM数据线5' },
  { gpio: 36, module: 'PSRAM', function: 'Octal PSRAM DQ6', type: 'Core', status: 'reserved', is_core: true, description: 'PSRAM数据线6' },
  { gpio: 37, module: 'PSRAM', function: 'Octal PSRAM DQ7', type: 'Core', status: 'reserved', is_core: true, description: 'PSRAM数据线7' },

  // Strapping 管脚
  { gpio: 0, module: '系统', function: 'Boot Strapping', type: 'RTC', status: 'reserved', warning: '上电启动配置管脚', description: 'GPIO0 / Boot选择' },
  { gpio: 3, module: '系统', function: 'Boot Strapping', type: 'RTC', status: 'reserved', warning: '上电启动配置管脚', description: 'GPIO3 / U0RXD' },
  { gpio: 45, module: '系统', function: 'Boot Strapping', type: 'RTC', status: 'reserved', warning: '上电启动配置管脚', description: 'GPIO45 / VDD_SPI电压配置' },
  { gpio: 46, module: '系统', function: 'Boot Strapping', type: 'RTC', status: 'reserved', warning: '上电启动配置管脚', description: 'GPIO46 / 强制下载模式' },

  // USB 调试管脚
  { gpio: 19, module: '系统', function: 'USB_D-', type: 'Core', status: 'reserved', description: 'USB JTAG/Serial 信号负' },
  { gpio: 20, module: '系统', function: 'USB_D+', type: 'Core', status: 'reserved', description: 'USB JTAG/Serial 信号正' },

  // 摄像头接口
  { gpio: 1, module: '摄像头', function: 'SCCB SDA', status: 'used', description: 'I2C 数据线，OV5640配置' },
  { gpio: 2, module: '摄像头', function: 'SCCB SCL', status: 'used', description: 'I2C 时钟线，OV5640配置' },
  { gpio: 3, module: '摄像头', function: 'VSYNC', status: 'used', description: '帧同步信号' },
  { gpio: 4, module: '摄像头', function: 'HREF', status: 'used', description: '行同步信号' },
  { gpio: 5, module: '摄像头', function: 'D2', status: 'used', description: 'DVP数据线 D2' },
  { gpio: 6, module: '摄像头', function: 'D3', status: 'used', description: 'DVP数据线 D3' },
  { gpio: 7, module: '摄像头', function: 'D4', status: 'used', description: 'DVP数据线 D4' },
  { gpio: 8, module: '摄像头', function: 'D5', status: 'used', description: 'DVP数据线 D5' },
  { gpio: 9, module: '摄像头', function: 'D6', status: 'used', description: 'DVP数据线 D6' },
  { gpio: 10, module: '摄像头', function: 'D7', status: 'used', description: 'DVP数据线 D7' },
  { gpio: 11, module: '摄像头', function: 'D8', status: 'used', description: 'DVP数据线 D8' },
  { gpio: 12, module: '摄像头', function: 'D9', status: 'used', description: 'DVP数据线 D9' },
  { gpio: 13, module: '摄像头', function: 'D10', status: 'used', description: 'DVP数据线 D10' },
  { gpio: 14, module: '摄像头', function: 'D11', status: 'used', description: 'DVP数据线 D11' },
  { gpio: 15, module: '摄像头', function: 'XCLK', status: 'used', description: '外部时钟 15MHz' },
  { gpio: 16, module: '摄像头', function: 'PCLK', status: 'used', description: '像素时钟' },

  // 执行器接口
  { gpio: 40, module: '执行器', function: '电机 PWMA', status: 'used', description: 'LEDC PWM电机调速' },
  { gpio: 41, module: '执行器', function: '电机 AIN1', type: 'RTC', status: 'used', description: '电机方向控制1' },
  { gpio: 42, module: '执行器', function: '电机 AIN2', type: 'RTC', status: 'used', description: '电机方向控制2' },
  { gpio: 17, module: '执行器', function: '舵机 SG90', status: 'used', description: '舵机PWM控制' },

  // 音频接口
  { gpio: 18, module: '音频', function: 'I2S BCLK', status: 'used', description: 'I2S 位时钟' },
  { gpio: 19, module: '音频', function: 'I2S WS', status: 'used', description: 'I2S 字选择/LRC' },
  { gpio: 20, module: '音频', function: 'I2S DIN', status: 'used', description: 'I2S 数据输入' },
  { gpio: 21, module: '音频', function: 'GAIN', status: 'used', description: '音频增益控制' },
  { gpio: 48, module: '音频', function: 'SD', status: 'used', description: '音频芯片关断控制' },

  // 传感器接口
  { gpio: 4, module: '传感器', function: 'ADC1_CH0', type: 'Analog', status: 'used', description: '热敏电阻 ADC' },
  { gpio: 5, module: '传感器', function: 'ADC1_CH1', type: 'Analog', status: 'used', description: '光敏电阻 ADC' },
  { gpio: 9, module: '传感器', function: 'Touch 3', type: 'Analog', status: 'used', description: 'DHT11 单总线温湿度' },

  // TF卡接口
  { gpio: 39, module: 'TF卡', function: 'SD CMD', type: 'RTC', status: 'used', description: 'SD卡命令线' },
  { gpio: 40, module: 'TF卡', function: 'SD CLK', type: 'RTC', status: 'used', description: 'SD卡时钟线' },
  { gpio: 41, module: 'TF卡', function: 'SD D0', status: 'used', description: 'SD卡数据线0' },
  { gpio: 42, module: 'TF卡', function: 'SD D1', status: 'used', description: 'SD卡数据线1' },
  { gpio: 43, module: 'TF卡', function: 'SD D2', status: 'used', description: 'SD卡数据线2' },
  { gpio: 44, module: 'TF卡', function: 'SD D3', status: 'used', description: 'SD卡数据线3' },

  // 可用 GPIO
  { gpio: 12, module: '', function: '', status: 'free', description: '可用GPIO，任意功能映射' },
  { gpio: 13, module: '', function: '', status: 'free', description: '可用GPIO，任意功能映射' },
  { gpio: 21, module: '', function: '', status: 'free', description: '可用GPIO，任意功能映射' },
  { gpio: 47, module: '', function: '', status: 'free', description: '可用GPIO，任意功能映射' },
]

// 初始化时按GPIO排序
pins.sort((a, b) => a.gpio - b.gpio)

// ========================================
// 生命周期
// ========================================
let timer = null

onMounted(() => {
  fetchBoardInfo()
  fetchTempData()
  fetchSystemStatus()
  timer = setInterval(() => {
    fetchTempData()
    fetchSystemStatus()
  }, 30000)
})

onUnmounted(() => {
  if (timer) clearInterval(timer)
})
</script>

<style scoped lang="scss">
@import '@/styles/variables';

.board-view {
  max-width: 100%;
}

.top-row {
  margin-bottom: $spacing-lg;
}

.image-container {
  display: flex;
  align-items: center;
  justify-content: center;
  min-height: 280px;
  background: $bg-color;
  border-radius: $border-radius-base;
  overflow: hidden;
}

.board-image {
  width: 100%;
  max-height: 320px;
}

.image-hint {
  display: flex;
  align-items: center;
  justify-content: center;
  gap: $spacing-xs;
  padding: $spacing-sm;
  font-size: $font-size-xs;
  color: $text-secondary;
  background: $bg-color;
  border-top: 1px solid $border-color-lighter;
}

// 温度显示
.temp-display {
  display: flex;
  align-items: center;
  gap: $spacing-xl;
  padding: $spacing-lg 0;

  @media (max-width: 768px) {
    flex-direction: column;
  }
}

.temp-gauge {
  flex-shrink: 0;
  text-align: center;
}

.temp-circle {
  width: 140px;
  height: 140px;
  border-radius: 50%;
  display: flex;
  align-items: center;
  justify-content: center;
  transition: all 0.3s ease;

  &.temp-normal {
    background: linear-gradient(135deg, rgba($success-color, 0.1), rgba($success-color, 0.2));
    border: 3px solid $success-color;
  }

  &.temp-warm {
    background: linear-gradient(135deg, rgba($warning-color, 0.1), rgba($warning-color, 0.2));
    border: 3px solid $warning-color;
  }

  &.temp-high {
    background: linear-gradient(135deg, rgba($danger-color, 0.1), rgba($danger-color, 0.2));
    border: 3px solid $danger-color;
  }

  &.temp-unknown {
    background: linear-gradient(135deg, rgba($text-secondary, 0.1), rgba($text-secondary, 0.2));
    border: 3px solid $border-color;
  }
}

.temp-inner {
  text-align: center;
}

.temp-value {
  font-size: 36px;
  font-weight: bold;
  color: $text-primary;
  line-height: 1;
}

.temp-unit {
  font-size: $font-size-md;
  color: $text-secondary;
}

.temp-label {
  margin-top: $spacing-md;
  font-size: $font-size-sm;
  color: $text-secondary;
}

.temp-info {
  flex: 1;
  min-width: 200px;
}

.text-success {
  color: $success-color;
}

.text-muted {
  color: $text-secondary;
  font-size: $font-size-sm;
}
</style>
