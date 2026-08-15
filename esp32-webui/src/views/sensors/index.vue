<template>
  <div class="sensors-view">
    <!-- 环境监测概览 -->
    <PageCard
      title="环境监测"
      icon="DataAnalysis"
      :refreshable="true"
      :loading="loading"
      @refresh="fetchData"
    >
      <el-row :gutter="20">
        <el-col :xs="12" :sm="8" v-for="item in sensorItems" :key="item.label">
          <DataCard
            :label="item.label"
            :value="item.value"
            :unit="item.unit"
            :decimals="item.decimals ?? 2"
          />
        </el-col>
      </el-row>
    </PageCard>

    <!-- 土壤湿度详情 -->
    <SoilHumidityCard :data="data.soilhumidity" :loading="loading" @refresh="fetchData" />

    <!-- 传感器配置 -->
    <PageCard title="传感器配置" icon="Setting">
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
          <StatusBadge :status="data.dht11?.valid" :text="data.dht11?.valid ? '在线' : '离线'" />
        </el-descriptions-item>
        <el-descriptions-item label="土壤湿度">
          GPIO {{ data.soilhumidity?.gpio || '--' }}
        </el-descriptions-item>
        <el-descriptions-item label="土壤湿度状态">
          <StatusBadge :status="data.soilhumidity?.humidity !== undefined ? 'success' : 'info'" :text="data.soilhumidity?.status || '未知'" />
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

      <div class="toggle-raw">
        <el-button type="text" @click="showRaw = !showRaw">
          {{ showRaw ? '隐藏' : '显示' }}原始数据
        </el-button>
      </div>
    </PageCard>
  </div>
</template>

<script setup>
import { ref, reactive, computed, onMounted, onUnmounted } from 'vue'
import { DataAnalysis, Setting } from '@element-plus/icons-vue'
import { PageCard, DataCard, StatusBadge } from '@/components/common'
import { SoilHumidityCard } from './components'
import { getSensorsDataSafe, getNetworkSafe } from '@/api/esp32'

const loading = ref(false)
const showRaw = ref(false)
const wifiRssi = ref(null)

const data = reactive({
  thermistor: null,
  photosensor: null,
  dht11: null,
  soilhumidity: null
})

const REFRESH_INTERVAL = 5000

// 传感器显示项
const sensorItems = computed(() => [
  { label: '热敏温度', value: data.thermistor?.temperature, unit: '°C', decimals: 2 },
  { label: '光敏亮度', value: data.photosensor?.light, unit: 'lux', decimals: 0 },
  { label: 'DHT11温度', value: data.dht11?.temperature, unit: '°C', decimals: 2 },
  { label: 'DHT11湿度', value: data.dht11?.humidity, unit: '%', decimals: 2 },
  { label: '土壤湿度', value: data.soilhumidity?.humidity, unit: '%', decimals: 2 },
  { label: 'WiFi信号', value: wifiRssi.value, unit: 'dBm', decimals: 0 }
])

// 使用安全的API调用，后端不可用时保持默认值
const fetchData = async () => {
  loading.value = true

  const [sensorsRes, networkRes] = await Promise.all([
    getSensorsDataSafe(),
    getNetworkSafe()
  ])

  // 更新传感器数据
  if (sensorsRes.data?.data) {
    Object.assign(data, sensorsRes.data.data)
  }

  // 更新网络数据
  if (networkRes.data?.rssi !== undefined) {
    wifiRssi.value = networkRes.data.rssi
  }

  loading.value = false
}

let refreshTimer = null

onMounted(() => {
  fetchData()
  refreshTimer = setInterval(fetchData, REFRESH_INTERVAL)
})

onUnmounted(() => {
  if (refreshTimer) {
    clearInterval(refreshTimer)
    refreshTimer = null
  }
})
</script>

<style scoped lang="scss">
@import '@/styles/variables';

.sensors-view {
  max-width: 100%;
}

.raw-data {
  margin-top: $spacing-base;
}

.toggle-raw {
  margin-top: $spacing-md;
}
</style>
