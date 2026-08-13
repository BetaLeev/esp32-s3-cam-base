<template>
  <div class="sensors-view">
    <el-card class="status-card">
      <template #header>
        <span>环境监测</span>
        <el-button :icon="Refresh" circle size="small" @click="fetchData" :loading="loading" />
      </template>
      <el-row :gutter="20">
        <el-col :span="8">
          <div class="sensor-item">
            <div class="sensor-icon">🌡️</div>
            <div class="sensor-value">{{ formatValue(data.thermistor?.temperature) }}<span class="unit">°C</span></div>
            <div class="sensor-label">热敏温度</div>
          </div>
        </el-col>
        <el-col :span="8">
          <div class="sensor-item">
            <div class="sensor-icon">💡</div>
            <div class="sensor-value">{{ formatValue(data.photosensor?.light, 0) }}<span class="unit">lux</span></div>
            <div class="sensor-label">光敏亮度</div>
          </div>
        </el-col>
        <el-col :span="8">
          <div class="sensor-item">
            <div class="sensor-icon">🌡️</div>
            <div class="sensor-value">{{ formatValue(data.dht11?.temperature) }}<span class="unit">°C</span></div>
            <div class="sensor-label">DHT11温度</div>
          </div>
        </el-col>
      </el-row>
      <el-row :gutter="20" style="margin-top: 20px">
        <el-col :span="8">
          <div class="sensor-item">
            <div class="sensor-icon">💧</div>
            <div class="sensor-value">{{ formatValue(data.dht11?.humidity) }}<span class="unit">%</span></div>
            <div class="sensor-label">DHT11湿度</div>
          </div>
        </el-col>
        <el-col :span="8">
          <div class="sensor-item">
            <div class="sensor-icon">🌱</div>
            <div class="sensor-value">{{ formatValue(data.soilhumidity?.humidity) }}<span class="unit">%</span></div>
            <div class="sensor-label">土壤湿度</div>
          </div>
        </el-col>
        <el-col :span="8">
          <div class="sensor-item">
            <div class="sensor-icon">📶</div>
            <div class="sensor-value">{{ wifiRssi ?? '--' }}<span class="unit">dBm</span></div>
            <div class="sensor-label">WiFi信号</div>
          </div>
        </el-col>
      </el-row>
    </el-card>

    <el-card class="soil-card">
      <template #header>
        <span>土壤湿度详情</span>
      </template>
      <SoilHumidityCard :data="data.soilhumidity" :loading="loading" @refresh="fetchData" />
    </el-card>

    <el-card class="config-card">
      <template #header>
        <span>传感器配置</span>
      </template>
      <el-descriptions :column="2" border>
        <el-descriptions-item label="热敏电阻">
          GPIO {{ data.thermistor?.gpio || '--' }}
        </el-descriptions-item>
        <el-descriptions-item label="光敏电阻">
          GPIO {{ data.photosensor?.gpio || '--' }}
        </el-descriptions-item>
        <el-descriptions-item label="DHT11">
          GPIO {{ data.dht11?.gpio || '--' }}
        </el-descriptions-item>
        <el-descriptions-item label="DHT11状态">
          <el-tag :type="data.dht11?.valid ? 'success' : 'danger'" size="small">
            {{ data.dht11?.valid ? '在线' : '离线' }}
          </el-tag>
        </el-descriptions-item>
        <el-descriptions-item label="土壤湿度">
          GPIO {{ data.soilhumidity?.gpio || '--' }}
        </el-descriptions-item>
        <el-descriptions-item label="土壤湿度状态">
          <el-tag :type="data.soilhumidity?.humidity !== undefined ? 'success' : 'info'" size="small">
            {{ data.soilhumidity?.status || '未知' }}
          </el-tag>
        </el-descriptions-item>
      </el-descriptions>
      <div class="raw-data" v-if="showRaw">
        <el-divider content-position="left">原始数据</el-divider>
        <el-descriptions :column="3" border size="small">
          <el-descriptions-item label="热敏原始值">
            {{ data.thermistor?.raw || 0 }}
          </el-descriptions-item>
          <el-descriptions-item label="光敏原始值">
            {{ data.photosensor?.raw || 0 }}
          </el-descriptions-item>
          <el-descriptions-item label="土壤湿度原始值">
            {{ data.soilhumidity?.raw || 0 }}
          </el-descriptions-item>
        </el-descriptions>
      </div>
      <div style="margin-top: 15px">
        <el-button type="text" @click="showRaw = !showRaw">
          {{ showRaw ? '隐藏' : '显示' }}原始数据
        </el-button>
      </div>
    </el-card>
  </div>
</template>

<script setup>
import { ref, reactive, onMounted, onUnmounted } from 'vue'
import { Refresh } from '@element-plus/icons-vue'
import { getSensorsData, getNetwork } from '@/api/esp32'
import SoilHumidityCard from './components/SoilHumidityCard.vue'

const loading = ref(false)
const showRaw = ref(false)
const wifiRssi = ref(null)
const data = reactive({
  thermistor: null,
  photosensor: null,
  dht11: null,
  soilhumidity: null
})

let refreshTimer = null
const REFRESH_INTERVAL = 5000 // 5秒

// 格式化数值，保留2位小数
const formatValue = (value, decimals = 2) => {
  if (value === null || value === undefined || value === '--') return '--'
  return Number(value).toFixed(decimals)
}

const fetchData = async () => {
  loading.value = true
  try {
    const [sensorsRes, networkRes] = await Promise.all([
      getSensorsData(),
      getNetwork()
    ])
    // sensorsRes.data 是 API 响应，sensorsRes.data.data 才是传感器数据
    if (sensorsRes.data?.data) {
      Object.assign(data, sensorsRes.data.data)
    }
    wifiRssi.value = networkRes.data?.rssi
  } catch (error) {
    console.error('获取传感器数据失败:', error)
  } finally {
    loading.value = false
  }
}

onMounted(() => {
  fetchData()
  // 启动自动刷新
  refreshTimer = setInterval(fetchData, REFRESH_INTERVAL)
})

onUnmounted(() => {
  // 清理定时器
  if (refreshTimer) {
    clearInterval(refreshTimer)
    refreshTimer = null
  }
})
</script>

<style scoped>
.sensors-view {
  max-width: 800px;
  margin: 0 auto;
}

.status-card, .config-card, .soil-card {
  margin-bottom: 20px;
}

.sensor-item {
  text-align: center;
  padding: 15px;
  background: #f5f7fa;
  border-radius: 8px;
}

.sensor-icon {
  font-size: 24px;
  margin-bottom: 8px;
}

.sensor-value {
  font-size: 24px;
  font-weight: bold;
  color: #303133;
}

.unit {
  font-size: 12px;
  color: #909399;
}

.sensor-label {
  margin-top: 4px;
  color: #909399;
  font-size: 12px;
}

.raw-data {
  margin-top: 15px;
}
</style>
