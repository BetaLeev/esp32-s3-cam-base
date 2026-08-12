<template>
  <el-card class="pump-card">
    <template #header>
      <div class="card-header">
        <span>水泵控制</span>
        <div class="header-right">
          <el-tag type="info" size="small" class="pin-tag">
            GPIO{{ pumpPin }}
          </el-tag>
          <el-tag :type="status.pump_state ? 'success' : 'info'" size="small">
            {{ status.pump_gear_name || '关闭' }}
          </el-tag>
        </div>
      </div>
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
</template>

<script setup>
import { ref, watch } from 'vue'
import { ElMessage } from 'element-plus'
import { controlPump } from '@/api/esp32'

const props = defineProps({
  status: {
    type: Object,
    default: () => ({})
  }
})

const pumpGear = ref(0)
const pumpPin = 40  // 水泵控制引脚

watch(() => props.status.pump_gear, (newVal) => {
  pumpGear.value = newVal ?? 0
}, { immediate: true })

const handlePumpChange = async (gear) => {
  try {
    await controlPump({ gear })
    ElMessage.success('水泵已切换')
  } catch (error) {
    ElMessage.error('控制失败')
  }
}
</script>

<style scoped>
.pump-card {
  margin-bottom: 20px;
}

.card-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
}

.header-right {
  display: flex;
  gap: 8px;
  align-items: center;
}

.pin-tag {
  background-color: #f0f2f5;
  border-color: #e4e7ed;
  color: #909399;
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
</style>
