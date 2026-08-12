<template>
  <el-card class="servo-card">
    <template #header>
      <div class="card-header">
        <span>舵机控制</span>
        <div class="header-right">
          <el-tag type="info" size="small" class="pin-tag">
            GPIO{{ servoPin }}
          </el-tag>
          <el-tag size="small">{{ status.servo_angle ?? 0 }}°</el-tag>
        </div>
      </div>
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
</template>

<script setup>
import { ref } from 'vue'
import { ElMessage } from 'element-plus'
import { controlServo } from '@/api/esp32'

const props = defineProps({
  status: {
    type: Object,
    default: () => ({})
  }
})

const servoAngle = ref(90)
const servoPin = 48  // 舵机控制引脚

const handleServoChange = async (angle) => {
  try {
    await controlServo(angle)
    ElMessage.success(`舵机已设置到 ${angle}°`)
  } catch (error) {
    ElMessage.error('控制失败')
  }
}
</script>

<style scoped>
.servo-card {
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
</style>
