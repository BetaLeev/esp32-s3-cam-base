<template>
  <div class="board-view">
    <!-- 第一行：图片 + 系统控制 + 温度 -->
    <el-row :gutter="16" class="top-row">
      <!-- 板子图片 -->
      <el-col :span="10">
        <BoardImage :image-url="imageUrl" />
      </el-col>

      <!-- 右侧：系统控制 + 温度 -->
      <el-col :span="14">
        <BoardControl :board-info="boardInfo" @reboot="onReboot" @shutdown="onShutdown" />
        <BoardTemp
          :loading="tempLoading"
          :temp-data="tempData"
          @refresh="fetchTempData"
        />
      </el-col>
    </el-row>

    <!-- 第二行：系统资源信息 -->
    <el-row>
      <el-col :span="24">
        <SystemResources :status="systemStatus" />
      </el-col>
    </el-row>

    <!-- 第三行：管脚列表 -->
    <el-row>
      <el-col :span="24">
        <PinList :pins="pins" />
      </el-col>
    </el-row>
  </div>
</template>

<script setup>
import { ref, reactive, onMounted, onUnmounted } from 'vue'
import BoardImage from './components/BoardImage.vue'
import BoardTemp from './components/BoardTemp.vue'
import BoardControl from './components/BoardControl.vue'
import PinList from './components/PinList.vue'
import SystemResources from '@/views/actuators/components/SystemResources.vue'
import { getBoardInfo, getBoardTemp, getStatus } from '@/api/esp32'

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

// 温度数据
const tempData = reactive({
  chip_temp: 0,
  ambient_temp: 0,
  cpu_temp: 0,
  sensor_ok: false
})

// 系统状态数据（用于系统资源显示）
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

const tempLoading = ref(false)
let tempTimer = null

// 获取板子信息
const fetchBoardInfo = async () => {
  try {
    const res = await getBoardInfo()
    if (res.data && res.data.data) {
      const data = res.data.data
      boardInfo.chip_model = data.chip_model || ''
      boardInfo.firmware_version = data.firmware_version || ''
      boardInfo.board_name = data.board_name || ''
      boardInfo.build_time = data.build_time || ''
      boardInfo.uptime = data.uptime || ''
      boardInfo.uptime_seconds = data.uptime_seconds || 0
      boardInfo.free_heap = data.free_heap || ''
    }
  } catch (error) {
    console.error('获取板子信息失败:', error)
  }
}

// 获取温度数据
const fetchTempData = async () => {
  tempLoading.value = true
  try {
    const res = await getBoardTemp()
    if (res.data && res.data.data) {
      const data = res.data.data
      tempData.chip_temp = data.chip_temp || 0
      tempData.ambient_temp = data.ambient_temp || 0
      tempData.cpu_temp = data.cpu_temp || 0
      tempData.sensor_ok = data.sensor_ok || false
    }
  } catch (error) {
    console.error('获取温度数据失败:', error)
  } finally {
    tempLoading.value = false
  }
}

// 获取系统状态数据
const fetchSystemStatus = async () => {
  try {
    const res = await getStatus()
    if (res.data) {
      // 更新系统资源数据
      systemStatus.dram_free = res.data.dram_free || 0
      systemStatus.dram_total = res.data.dram_total || 0
      systemStatus.psram_free = res.data.psram_free || 0
      systemStatus.psram_total = res.data.psram_total || 0
      systemStatus.flash_total = res.data.flash_total || 0
      systemStatus.spiffs_free = res.data.spiffs_free || 0
      systemStatus.spiffs_total = res.data.spiffs_total || 0
      systemStatus.sdcard_mounted = res.data.sdcard_mounted || false
      systemStatus.sdcard_free = res.data.sdcard_free || 0
      systemStatus.uptime_seconds = res.data.uptime_seconds || 0
    }
  } catch (error) {
    console.error('获取系统状态失败:', error)
  }
}

// 系统控制事件处理
const onReboot = () => {
  console.log('Reboot triggered')
}

const onShutdown = () => {
  console.log('Shutdown triggered')
}

// ========================================
// 管脚数据定义 (静态数据)
// ========================================
const pins = [
  // ========== 核心/系统占用管脚 ==========
  { gpio: 26, module: 'SPI Flash', function: 'SPI0/1 Flash', type: 'Core', status: 'reserved', is_core: true, warning: '系统核心管脚，不可作为GPIO使用', description: '片内Flash接口，系统启动必需' },
  { gpio: 27, module: 'SPI Flash', function: 'SPI0/1 Flash', type: 'Core', status: 'reserved', is_core: true, warning: '系统核心管脚，不可作为GPIO使用', description: '片内Flash接口，系统启动必需' },
  { gpio: 28, module: 'SPI Flash', function: 'SPI0/1 Flash', type: 'Core', status: 'reserved', is_core: true, warning: '系统核心管脚，不可作为GPIO使用', description: '片内Flash接口，系统启动必需' },
  { gpio: 29, module: 'SPI Flash', function: 'SPI0/1 Flash', type: 'Core', status: 'reserved', is_core: true, warning: '系统核心管脚，不可作为GPIO使用', description: '片内Flash接口，系统启动必需' },
  { gpio: 30, module: 'PSRAM', function: 'Octal PSRAM DQ0', type: 'Core', status: 'reserved', is_core: true, warning: 'PSRAM数据线，不可作为GPIO使用', description: 'PSRAM数据线0' },
  { gpio: 31, module: 'PSRAM', function: 'Octal PSRAM DQ1', type: 'Core', status: 'reserved', is_core: true, warning: 'PSRAM数据线，不可作为GPIO使用', description: 'PSRAM数据线1' },
  { gpio: 32, module: 'PSRAM', function: 'Octal PSRAM DQ2', type: 'Core', status: 'reserved', is_core: true, warning: 'PSRAM数据线，不可作为GPIO使用', description: 'PSRAM数据线2' },
  { gpio: 33, module: 'PSRAM', function: 'Octal PSRAM DQ3', type: 'Core', status: 'reserved', is_core: true, warning: 'PSRAM数据线，不可作为GPIO使用', description: 'PSRAM数据线3' },
  { gpio: 34, module: 'PSRAM', function: 'Octal PSRAM DQ4', type: 'Core', status: 'reserved', is_core: true, warning: 'PSRAM数据线，不可作为GPIO使用', description: 'PSRAM数据线4' },
  { gpio: 35, module: 'PSRAM', function: 'Octal PSRAM DQ5', type: 'Core', status: 'reserved', is_core: true, warning: 'PSRAM数据线，不可作为GPIO使用', description: 'PSRAM数据线5' },
  { gpio: 36, module: 'PSRAM', function: 'Octal PSRAM DQ6', type: 'Core', status: 'reserved', is_core: true, warning: 'PSRAM数据线，不可作为GPIO使用', description: 'PSRAM数据线6' },
  { gpio: 37, module: 'PSRAM', function: 'Octal PSRAM DQ7', type: 'Core', status: 'reserved', is_core: true, warning: 'PSRAM数据线，不可作为GPIO使用', description: 'PSRAM数据线7' },

  // ========== Strapping 管脚 ==========
  { gpio: 0, module: '系统', function: 'Boot Strapping', type: 'RTC', status: 'reserved', warning: '上电启动配置管脚，上电期间勿改变电平', description: 'GPIO0 / Boot选择 / ROM日志' },
  { gpio: 3, module: '系统', function: 'Boot Strapping', type: 'RTC', status: 'reserved', warning: '上电启动配置管脚', description: 'GPIO3 / U0RXD / Boot配置' },
  { gpio: 45, module: '系统', function: 'Boot Strapping', type: 'RTC', status: 'reserved', warning: '上电启动配置管脚', description: 'GPIO45 / VDD_SPI电压配置' },
  { gpio: 46, module: '系统', function: 'Boot Strapping', type: 'RTC', status: 'reserved', warning: '上电启动配置管脚', description: 'GPIO46 / 强制下载模式' },

  // ========== USB 调试管脚 ==========
  { gpio: 19, module: '系统', function: 'USB_D-', type: 'Core', status: 'reserved', warning: 'USB调试接口，用于程序下载和日志输出', description: 'USB JTAG/Serial 信号负' },
  { gpio: 20, module: '系统', function: 'USB_D+', type: 'Core', status: 'reserved', warning: 'USB调试接口，用于程序下载和日志输出', description: 'USB JTAG/Serial 信号正' },

  // ========== RTC 晶振管脚（未使用外部晶振时可用） ==========
  { gpio: 15, module: 'RTC', function: 'XTAL_32K_P', type: 'RTC', status: 'free', description: '32.768kHz晶振正极（未接晶振时可用）' },
  { gpio: 16, module: 'RTC', function: 'XTAL_32K_N', type: 'RTC', status: 'free', description: '32.768kHz晶振负极（未接晶振时可用）' },

  // ========== 摄像头接口 ==========
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

  // ========== 执行器接口 ==========
  { gpio: 40, module: '执行器', function: '电机 PWMA', status: 'used', description: 'LEDC PWM电机调速' },
  { gpio: 41, module: '执行器', function: '电机 AIN1', type: 'RTC', status: 'used', description: '电机方向控制1' },
  { gpio: 42, module: '执行器', function: '电机 AIN2', type: 'RTC', status: 'used', description: '电机方向控制2' },
  { gpio: 17, module: '执行器', function: '舵机 SG90', status: 'used', description: '舵机PWM控制' },

  // ========== 音频接口 ==========
  { gpio: 18, module: '音频', function: 'I2S BCLK', status: 'used', description: 'I2S 位时钟' },
  { gpio: 19, module: '音频', function: 'I2S WS', status: 'used', description: 'I2S 字选择/LRC' },
  { gpio: 20, module: '音频', function: 'I2S DIN', status: 'used', description: 'I2S 数据输入' },
  { gpio: 21, module: '音频', function: 'GAIN', status: 'used', description: '音频增益控制' },
  { gpio: 48, module: '音频', function: 'SD', status: 'used', description: '音频芯片关断控制' },

  // ========== 传感器接口 ==========
  { gpio: 4, module: '传感器', function: 'ADC1_CH0', type: 'Analog', status: 'used', description: '热敏电阻 ADC' },
  { gpio: 5, module: '传感器', function: 'ADC1_CH1', type: 'Analog', status: 'used', description: '光敏电阻 ADC' },
  { gpio: 6, module: '传感器', function: 'Touch 0', type: 'Analog', status: 'free', description: '电容触摸通道0（未使用）' },
  { gpio: 7, module: '传感器', function: 'Touch 1', type: 'Analog', status: 'free', description: '电容触摸通道1（未使用）' },
  { gpio: 8, module: '传感器', function: 'Touch 2', type: 'Analog', status: 'free', description: '电容触摸通道2（未使用）' },
  { gpio: 9, module: '传感器', function: 'Touch 3', type: 'Analog', status: 'used', description: 'DHT11 单总线温湿度' },

  // ========== TF卡接口 ==========
  { gpio: 39, module: 'TF卡', function: 'SD CMD', type: 'RTC', status: 'used', description: 'SD卡命令线' },
  { gpio: 40, module: 'TF卡', function: 'SD CLK', type: 'RTC', status: 'used', description: 'SD卡时钟线' },
  { gpio: 41, module: 'TF卡', function: 'SD D0', status: 'used', description: 'SD卡数据线0' },
  { gpio: 42, module: 'TF卡', function: 'SD D1', status: 'used', description: 'SD卡数据线1' },
  { gpio: 43, module: 'TF卡', function: 'SD D2', status: 'used', description: 'SD卡数据线2' },
  { gpio: 44, module: 'TF卡', function: 'SD D3', status: 'used', description: 'SD卡数据线3' },

  // ========== 系统串口 ==========
  { gpio: 43, module: '系统', function: 'UART0 TX', status: 'used', description: '串口0发送（控制台）' },
  { gpio: 44, module: '系统', function: 'UART0 RX', status: 'used', description: '串口0接收（控制台）' },

  // ========== 可用 GPIO ==========
  { gpio: 12, module: '', function: '', status: 'free', description: '可用GPIO，任意功能映射' },
  { gpio: 13, module: '', function: '', status: 'free', description: '可用GPIO，任意功能映射' },
  { gpio: 21, module: '', function: '', status: 'free', description: '可用GPIO，任意功能映射' },
  { gpio: 47, module: '', function: '', status: 'free', description: '可用GPIO，任意功能映射' },
]

// 初始化时按GPIO排序
pins.sort((a, b) => a.gpio - b.gpio)

onMounted(() => {
  fetchBoardInfo()
  fetchTempData()
  fetchSystemStatus()
  tempTimer = setInterval(() => {
    fetchTempData()
    fetchSystemStatus()
  }, 30000)
})

onUnmounted(() => {
  if (tempTimer) clearInterval(tempTimer)
})
</script>

<style scoped>
.board-view {
  max-width: 1400px;
  margin: 0 auto;
}

.top-row {
  margin-bottom: 20px;
}
</style>
