<template>
  <PageCard title="土壤湿度详情" icon="Sunny" :refreshable="true" :loading="loading" @refresh="$emit('refresh')">
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
  </PageCard>
</template>

<script setup>
import { computed } from 'vue'
import { Sunny } from '@element-plus/icons-vue'
import { PageCard } from '@/components/common'

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

<style scoped lang="scss">
@import '@/styles/variables';

.humidity-display {
  display: flex;
  justify-content: center;
  padding: $spacing-lg 0;
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
  border-color: $danger-color;
  background-color: #fef0f0;
}

.humidity-ring.status-light-dry {
  border-color: $warning-color;
  background-color: #fdf6ec;
}

.humidity-ring.status-optimal {
  border-color: $success-color;
  background-color: #f0f9eb;
}

.humidity-ring.status-light-wet {
  border-color: $primary-color;
  background-color: #ecf5ff;
}

.humidity-ring.status-wet {
  border-color: #1e90ff;
  background-color: #e6f7ff;
}

.humidity-value {
  display: flex;
  align-items: baseline;
}

.humidity-value .value {
  font-size: 32px;
  font-weight: bold;
  color: $text-primary;
}

.humidity-value .unit {
  font-size: $font-size-md;
  color: $text-secondary;
  margin-left: 2px;
}

.humidity-status {
  font-size: $font-size-sm;
  font-weight: 500;
  margin-top: $spacing-xs;
  padding: 2px 8px;
  border-radius: $border-radius-small;
}

.humidity-status.status-dry {
  color: $danger-color;
  background-color: #fef0f0;
}

.humidity-status.status-light-dry {
  color: $warning-color;
  background-color: #fdf6ec;
}

.humidity-status.status-optimal {
  color: $success-color;
  background-color: #f0f9eb;
}

.humidity-status.status-light-wet {
  color: $primary-color;
  background-color: #ecf5ff;
}

.humidity-status.status-wet {
  color: #1e90ff;
  background-color: #e6f7ff;
}

.raw-info {
  margin-bottom: $spacing-base;
}

.moisture-scale {
  margin-top: $spacing-base;
}

.scale-label {
  font-size: $font-size-xs;
  color: $text-secondary;
  margin-bottom: $spacing-sm;
}

.scale-legend {
  display: flex;
  justify-content: space-between;
  margin-top: $spacing-sm;
  font-size: 11px;
  color: $text-regular;
}

.legend-item {
  padding: 2px 6px;
  border-radius: $border-radius-small;
}

.legend-item.dry {
  color: $danger-color;
  background-color: #fef0f0;
}

.legend-item.optimal {
  color: $success-color;
  background-color: #f0f9eb;
}

.legend-item.wet {
  color: $primary-color;
  background-color: #ecf5ff;
}
</style>
