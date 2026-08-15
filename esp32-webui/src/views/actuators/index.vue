<template>
  <div class="actuators-view">
    <!-- 控制面板标题 -->
    <div class="panel-header">
      <div class="header-left">
        <h2 class="panel-title">设备控制</h2>
        <span class="panel-subtitle"> actuators control panel</span>
      </div>
      <div class="header-right">
        <el-tag type="info" size="small">
          <el-icon class="el-icon--left"><Timer /></el-icon>
          每5秒自动刷新
        </el-tag>
      </div>
    </div>

    <!-- 执行器网格 -->
    <div class="actuators-grid">
      <!-- 第一行：水泵 + 舵机 -->
      <div class="actuator-row">
        <PumpControl :status="status" />
        <ServoControl :status="status" />
      </div>

      <!-- 第二行：LED + 脉冲 -->
      <div class="actuator-row">
        <LedControl :status="status" :usedPins="usedPins" />
        <PulseControl :status="status" :usedPins="usedPins" />
      </div>

      <!-- 第三行：音频控制 -->
      <div class="actuator-row">
        <AudioControl />
        <div></div>
      </div>
    </div>
  </div>
</template>

<script setup>
import { reactive, ref, onMounted, onUnmounted } from 'vue'
import { Timer } from '@element-plus/icons-vue'
import { PumpControl, ServoControl, LedControl, PulseControl, AudioControl } from './components'
import { getStatusSafe, getUsedPins } from '@/api/esp32'

const status = reactive({
  pump_state: false,
  pump_gear_name: '',
  pump_gear: 0,
  pump_speed: 0,
  servo_angle: 90,
  dram_free: 0,
  dram_total: 0,
  psram_free: 0,
  psram_total: 0,
  flash_total: 0,
  spiffs_free: 0,
  spiffs_total: 0,
  sdcard_mounted: false,
  sdcard_free: 0,
  uptime_seconds: 0,
  led_pin: 2,
  led_enabled: false,
  led_current_level: 0,
  led_executed_count: 0,
  led_total_count: 3,
  led_elapsed_time: 0,
  led_remaining_time: 0,
  pulse_pin: 2,
  pulse_enabled: false,
  pulse_current_intensity: 0,
  pulse_count: 0,
  pulse_elapsed_time: 0,
  pulse_pin_level: 0
})

const usedPins = ref([4, 5, 12])
let timer = null

// 使用安全的API调用，后端不可用时保持默认值
const fetchStatus = async () => {
  const { data } = await getStatusSafe()
  if (data) {
    Object.keys(data).forEach(key => {
      if (key in status) {
        status[key] = data[key]
      }
    })
  }
}

const fetchUsedPins = async () => {
  try {
    const res = await getUsedPins()
    usedPins.value = res.data?.pins || []
  } catch (error) {
    // 忽略错误，使用默认值
  }
}

onMounted(() => {
  fetchStatus()
  fetchUsedPins()
  timer = setInterval(fetchStatus, 5000)
})

onUnmounted(() => {
  if (timer) clearInterval(timer)
})
</script>

<style scoped lang="scss">
@import '@/styles/variables';

.actuators-view {
  max-width: 100%;
}

// 面板标题
.panel-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin-bottom: $spacing-lg;
  padding: $spacing-md $spacing-lg;
  background: linear-gradient(135deg, $primary-color 0%, darken($primary-color, 10%) 100%);
  border-radius: $border-radius-base;
  color: #fff;
}

.panel-title {
  margin: 0;
  font-size: $font-size-xl;
  font-weight: 600;
}

.panel-subtitle {
  font-size: $font-size-sm;
  opacity: 0.8;
}

.header-right .el-tag {
  background: rgba(255, 255, 255, 0.2);
  border-color: transparent;
  color: #fff;
}

// 执行器网格
.actuators-grid {
  display: flex;
  flex-direction: column;
  gap: $spacing-lg;
}

.actuator-row {
  display: grid;
  grid-template-columns: repeat(2, 1fr);
  gap: $spacing-lg;

  @media (max-width: 960px) {
    grid-template-columns: 1fr;
  }
}
</style>
