<template>
  <div class="soil-humidity-card">
    <el-card shadow="hover">
      <template #header>
        <div class="card-header">
          <span>土壤湿度监测</span>
          <el-button :icon="Refresh" circle size="small" @click="$emit('refresh')" :loading="loading" />
        </div>
      </template>

      <div class="humidity-display">
        <div class="humidity-ring" :class="statusClass">
          <div class="humidity-value">
            <span class="value">{{ formatValue(data?.humidity) }}</span>
            <span class="unit">%</span>
          </div>
          <div class="humidity-status" :class="statusClass">{{ statusText }}</div>
        </div>
      </div>

      <el-divider />

      <div class="raw-info">
        <el-descriptions :column="2" border size="small">
          <el-descriptions-item label="GPIO">
            <el-tag type="info" size="small">{{ data?.gpio || '--' }}</el-tag>
          </el-descriptions-item>
          <el-descriptions-item label="ADC原始值">
            <el-tag type="info" size="small">{{ data?.raw || 0 }}</el-tag>
          </el-descriptions-item>
        </el-descriptions>
      </div>

      <div class="moisture-scale">
        <div class="scale-label">湿度等级</div>
        <el-progress :percentage="data?.humidity || 0" :color="scaleColor" :stroke-width="12" />
        <div class="scale-legend">
          <span class="legend-item dry">干燥 &lt;20%</span>
          <span class="legend-item optimal">适中 40-60%</span>
          <span class="legend-item wet">湿润 &gt;80%</span>
        </div>
      </div>
    </el-card>
  </div>
</template>

<script setup>
import { computed } from 'vue'
import { Refresh } from '@element-plus/icons-vue'

const props = defineProps({
  data: {
    type: Object,
    default: null
  },
  loading: {
    type: Boolean,
    default: false
  }
})

defineEmits(['refresh'])

const statusClass = computed(() => {
  const humidity = props.data?.humidity || 0
  if (humidity < 20) return 'status-dry'
  if (humidity < 40) return 'status-light-dry'
  if (humidity < 60) return 'status-optimal'
  if (humidity < 80) return 'status-light-wet'
  return 'status-wet'
})

const statusText = computed(() => {
  return props.data?.status || '未知'
})

const scaleColor = computed(() => {
  const humidity = props.data?.humidity || 0
  if (humidity < 20) return '#E6A23C'
  if (humidity < 40) return '#E6A23C'
  if (humidity < 60) return '#67C23A'
  if (humidity < 80) return '#409EFF'
  return '#409EFF'
})

const formatValue = (value, decimals = 1) => {
  if (value === null || value === undefined || value === '--') return '--'
  return Number(value).toFixed(decimals)
}
</script>

<style scoped>
.soil-humidity-card {
  width: 100%;
}

.card-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
}

.humidity-display {
  display: flex;
  justify-content: center;
  padding: 20px 0;
}

.humidity-ring {
  width: 140px;
  height: 140px;
  border-radius: 50%;
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  border: 6px solid;
  transition: all 0.3s ease;
}

.humidity-ring.status-dry {
  border-color: #F56C6C;
  background-color: #FEF0F0;
}

.humidity-ring.status-light-dry {
  border-color: #E6A23C;
  background-color: #FCF6EC;
}

.humidity-ring.status-optimal {
  border-color: #67C23A;
  background-color: #F0F9EB;
}

.humidity-ring.status-light-wet {
  border-color: #409EFF;
  background-color: #ECF5FF;
}

.humidity-ring.status-wet {
  border-color: #1E90FF;
  background-color: #E6F7FF;
}

.humidity-value {
  display: flex;
  align-items: baseline;
}

.humidity-value .value {
  font-size: 32px;
  font-weight: bold;
  color: #303133;
}

.humidity-value .unit {
  font-size: 16px;
  color: #909399;
  margin-left: 2px;
}

.humidity-status {
  font-size: 14px;
  font-weight: 500;
  margin-top: 4px;
  padding: 2px 8px;
  border-radius: 4px;
}

.humidity-status.status-dry {
  color: #F56C6C;
  background-color: #FEF0F0;
}

.humidity-status.status-light-dry {
  color: #E6A23C;
  background-color: #FCF6EC;
}

.humidity-status.status-optimal {
  color: #67C23A;
  background-color: #F0F9EB;
}

.humidity-status.status-light-wet {
  color: #409EFF;
  background-color: #ECF5FF;
}

.humidity-status.status-wet {
  color: #1E90FF;
  background-color: #E6F7FF;
}

.raw-info {
  margin-bottom: 15px;
}

.moisture-scale {
  margin-top: 15px;
}

.scale-label {
  font-size: 12px;
  color: #909399;
  margin-bottom: 8px;
}

.scale-legend {
  display: flex;
  justify-content: space-between;
  margin-top: 8px;
  font-size: 11px;
  color: #606266;
}

.legend-item {
  padding: 2px 6px;
  border-radius: 3px;
}

.legend-item.dry {
  color: #F56C6C;
  background-color: #FEF0F0;
}

.legend-item.optimal {
  color: #67C23A;
  background-color: #F0F9EB;
}

.legend-item.wet {
  color: #409EFF;
  background-color: #ECF5FF;
}
</style>
