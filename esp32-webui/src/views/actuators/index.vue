<template>
  <div class="actuators-view">
    <PumpControl :status="status" />
    <ServoControl :status="status" />
    <LedControl :status="status" :usedPins="usedPins" />
    <PulseControl :status="status" :usedPins="usedPins" />
  </div>
</template>

<script setup>
import { reactive, ref, onMounted, onUnmounted } from 'vue'
import PumpControl from './components/PumpControl.vue'
import ServoControl from './components/ServoControl.vue'
import LedControl from './components/LedControl.vue'
import PulseControl from './components/PulseControl.vue'
import { getStatus, getUsedPins } from '@/api/esp32'

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
  // LED 状态
  led_pin: 2,
  led_enabled: false,
  led_current_level: 0,
  led_executed_count: 0,
  led_total_count: 3,
  led_elapsed_time: 0,
  led_remaining_time: 0,
  // 脉冲状态
  pulse_pin: 2,
  pulse_enabled: false,
  pulse_current_intensity: 0,
  pulse_count: 0,
  pulse_elapsed_time: 0,
  pulse_pin_level: 0
})

const usedPins = ref([4, 5, 12]) // 模拟已被占用的引脚
let timer = null

const fetchStatus = async () => {
  try {
    const res = await getStatus()
    Object.keys(res.data).forEach(key => {
      if (key in status) {
        status[key] = res.data[key]
      }
    })
  } catch (error) {
    console.error('获取状态失败:', error)
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

<style scoped>
.actuators-view {
  max-width: 800px;
  margin: 0 auto;
}
</style>
