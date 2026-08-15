<template>
  <PageCard title="水泵控制" icon="Lightning">
    <template #header-extra>
      <StatusBadge :status="pumpGear > 0" :text="gearName" />
    </template>

    <div class="pump-content">
      <!-- 管脚信息 -->
      <div class="pin-info">
        <el-descriptions :column="3" :gutter="20">
          <el-descriptions-item label="PWM引脚">GPIO1</el-descriptions-item>
          <el-descriptions-item label="方向引脚">GPIO2 + GPIO42</el-descriptions-item>
          <el-descriptions-item label="驱动芯片">TB6612</el-descriptions-item>
        </el-descriptions>
      </div>

      <!-- 档位选择 -->
      <div class="gear-section">
        <div class="section-label">档位选择</div>
        <el-radio-group v-model="pumpGear" @change="handleChange" class="gear-buttons">
          <el-radio-button :value="0">关闭</el-radio-button>
          <el-radio-button :value="1">低速</el-radio-button>
          <el-radio-button :value="2">中速</el-radio-button>
          <el-radio-button :value="3">高速</el-radio-button>
        </el-radio-group>
      </div>

      <!-- 当前状态 -->
      <div v-if="pumpSpeed > 0" class="status-card">
        <el-icon class="status-icon"><Lightning /></el-icon>
        <div class="status-info">
          <div class="status-speed">{{ pumpSpeed }}%</div>
          <div class="status-label">当前速度</div>
        </div>
      </div>
    </div>
  </PageCard>
</template>

<script setup>
import { ref, watch, computed } from 'vue'
import { Lightning } from '@element-plus/icons-vue'
import { PageCard, StatusBadge } from '@/components/common'
import { controlPump } from '@/api/esp32'

const props = defineProps({
  status: { type: Object, default: () => ({}) }
})

const pumpGear = ref(0)
const pumpSpeed = ref(0)

const gearName = computed(() => {
  switch (pumpGear.value) {
    case 1: return '低速'
    case 2: return '中速'
    case 3: return '高速'
    default: return '关闭'
  }
})

watch(() => props.status.pump_gear, (val) => {
  pumpGear.value = val ?? 0
  pumpSpeed.value = props.status.pump_speed ?? 0
}, { immediate: true })

watch(() => props.status.pump_speed, (val) => {
  pumpSpeed.value = val ?? 0
})

const handleChange = async (gear) => {
  try {
    await controlPump({ gear })
  } catch (e) {
    console.error('水泵控制失败:', e)
  }
}
</script>

<style scoped lang="scss">
.pump-content {
  padding: 16px 0;
}
.pin-info {
  margin-bottom: 16px;
}
.section-label {
  font-size: 13px;
  color: #909399;
  margin-bottom: 12px;
  text-align: center;
}
.gear-buttons {
  display: flex;
}
.gear-buttons :deep(.el-radio-button__inner) {
  padding: 10px 20px;
}
.status-card {
  display: flex;
  align-items: center;
  justify-content: center;
  gap: 16px;
  margin-top: 24px;
  padding: 16px;
  background: #f5f7fa;
  border-radius: 8px;
}
.status-icon {
  font-size: 32px;
  color: #909399;
}
.status-info {
  text-align: left;
}
.status-speed {
  font-size: 28px;
  font-weight: bold;
  color: #303133;
}
.status-label {
  font-size: 13px;
  color: #909399;
}
</style>
