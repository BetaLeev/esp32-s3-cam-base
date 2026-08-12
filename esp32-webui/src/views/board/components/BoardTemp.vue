<template>
  <el-card class="board-temp-card">
    <template #header>
      <div class="card-header">
        <span>板子温度监控</span>
        <el-button type="primary" size="small" plain @click="$emit('refresh')" :loading="loading">
          <el-icon><Refresh /></el-icon>
          刷新
        </el-button>
      </div>
    </template>

    <!-- 温度卡片 -->
    <el-row :gutter="16" class="temp-row">
      <el-col :span="8" v-for="item in tempItems" :key="item.label">
        <div class="temp-item" :class="item.class">
          <div class="temp-value">
            <span class="value">{{ item.value }}</span>
            <span class="unit">°C</span>
          </div>
          <div class="temp-label">{{ item.label }}</div>
          <div class="temp-bar">
            <div class="temp-bar-fill" :style="{ width: getBarWidth(item.value) + '%' }"></div>
          </div>
        </div>
      </el-col>
    </el-row>

    <!-- 温度状态提示 -->
    <div class="temp-status" v-if="status">
      <el-alert
        :title="status.message"
        :type="status.type"
        :closable="false"
        show-icon
      />
    </div>

    <!-- 传感器状态 -->
    <div class="sensor-info" v-if="tempData">
      <el-tag v-if="tempData.sensor_ok" type="success" size="small">
        温度传感器正常
      </el-tag>
      <el-tag v-else type="danger" size="small">
        温度传感器未初始化
      </el-tag>
    </div>
  </el-card>
</template>

<script setup>
import { computed } from 'vue'
import { Refresh } from '@element-plus/icons-vue'

const props = defineProps({
  loading: {
    type: Boolean,
    default: false
  },
  tempData: {
    type: Object,
    default: () => ({
      chip_temp: 0,
      ambient_temp: 0,
      cpu_temp: 0,
      sensor_ok: false
    })
  }
})

defineEmits(['refresh'])

const tempItems = computed(() => [
  { label: '芯片温度', value: (props.tempData.chip_temp || 0).toFixed(1), class: 'temp-chip' },
  { label: '环境温度', value: (props.tempData.ambient_temp || 0).toFixed(1), class: 'temp-ambient' },
  { label: 'CPU温度', value: (props.tempData.cpu_temp || 0).toFixed(1), class: 'temp-cpu' }
])

const status = computed(() => {
  const temp = parseFloat(props.tempData.chip_temp) || 0
  if (temp >= 80) {
    return { type: 'error', message: '温度过高！请检查散热或降低负载' }
  } else if (temp >= 60) {
    return { type: 'warning', message: '温度偏高，建议加强散热' }
  } else if (temp > 0) {
    return { type: 'success', message: '温度正常' }
  }
  return null
})

const getBarWidth = (value) => {
  return Math.min(100, Math.max(0, parseFloat(value) || 0))
}
</script>

<style scoped>
.board-temp-card {
  margin-bottom: 20px;
}

.card-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
}

.temp-row {
  margin-bottom: 16px;
}

.temp-item {
  padding: 16px;
  background: #f5f7fa;
  border-radius: 8px;
  text-align: center;
}

.temp-value {
  font-size: 28px;
  font-weight: bold;
}

.temp-value .value {
  color: #303133;
}

.temp-value .unit {
  font-size: 16px;
  color: #909399;
  margin-left: 2px;
}

.temp-label {
  margin-top: 4px;
  font-size: 13px;
  color: #606266;
}

.temp-bar {
  margin-top: 12px;
  height: 6px;
  background: #e4e7ed;
  border-radius: 3px;
  overflow: hidden;
}

.temp-bar-fill {
  height: 100%;
  background: linear-gradient(90deg, #67c23a, #e6a23c, #f56c6c);
  border-radius: 3px;
  transition: width 0.3s ease;
}

/* 温度卡片颜色 */
.temp-chip .temp-value .value { color: #409eff; }
.temp-ambient .temp-value .value { color: #67c23a; }
.temp-cpu .temp-value .value { color: #e6a23c; }

.temp-status {
  margin-top: 16px;
}

.sensor-info {
  margin-top: 12px;
  text-align: center;
}
</style>
