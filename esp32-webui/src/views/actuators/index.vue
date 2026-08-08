<template>
  <div class="actuators-view">
    <el-card class="pump-card">
      <template #header>
        <span>水泵控制</span>
        <el-tag :type="status.pump_state ? 'success' : 'info'" size="small">
          {{ status.pump_gear_name || '关闭' }}
        </el-tag>
      </template>
      <div class="control-content">
        <el-radio-group v-model="pumpGear" @change="handlePumpChange">
          <el-radio-button :value="0">关闭</el-radio-button>
          <el-radio-button :value="1">低速</el-radio-button>
          <el-radio-button :value="2">中速</el-radio-button>
          <el-radio-button :value="3">高速</el-radio-button>
        </el-radio-group>
        <div class="pump-speed" v-if="status.pump_speed">
          当前速度: {{ status.pump_speed }}%
        </div>
      </div>
    </el-card>

    <el-card class="servo-card">
      <template #header>
        <span>舵机控制</span>
        <el-tag size="small">{{ status.servo_angle ?? 0 }}°</el-tag>
      </template>
      <div class="control-content">
        <el-slider
          v-model="servoAngle"
          :min="0"
          :max="180"
          :step="5"
          show-stops
          @change="handleServoChange"
        />
        <el-button type="primary" @click="handleServoChange(servoAngle)">
          设置角度
        </el-button>
      </div>
    </el-card>

    <el-card class="system-card">
      <template #header>
        <span>系统资源</span>
      </template>
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
    </el-card>
  </div>
</template>

<script setup>
import { ref, reactive, onMounted, onUnmounted } from 'vue'
import { ElMessage } from 'element-plus'
import { getStatus, controlPump, controlServo } from '@/api/esp32'

const status = reactive({})
const pumpGear = ref(0)
const servoAngle = ref(90)
let timer = null

const fetchStatus = async () => {
  try {
    const { data } = await getStatus()
    Object.assign(status, data)
    pumpGear.value = data.pump_gear ?? 0
  } catch (error) {
    console.error('获取状态失败:', error)
  }
}

const handlePumpChange = async (gear) => {
  try {
    await controlPump({ gear })
    ElMessage.success('水泵已切换')
    fetchStatus()
  } catch (error) {
    ElMessage.error('控制失败')
  }
}

const handleServoChange = async (angle) => {
  try {
    await controlServo(angle)
    ElMessage.success(`舵机已设置到 ${angle}°`)
  } catch (error) {
    ElMessage.error('控制失败')
  }
}

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

onMounted(() => {
  fetchStatus()
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

.pump-card, .servo-card, .system-card {
  margin-bottom: 20px;
}

.control-content {
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 15px;
  padding: 15px 0;
}

.pump-speed {
  color: #67c23a;
  font-size: 14px;
}

.text-success {
  color: #67c23a;
}

.text-muted {
  color: #909399;
}
</style>
