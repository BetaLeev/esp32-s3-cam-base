<template>
  <PageCard title="舵机控制" icon="Rank" class="servo-card">
    <template #header-extra>
      <el-tag size="small" type="primary">{{ servoAngle }}°</el-tag>
    </template>

    <div class="control-content">
      <!-- 管脚信息 -->
      <div class="pin-info">
        <el-descriptions :column="3" :gutter="20">
          <el-descriptions-item label="PWM引脚">GPIO48</el-descriptions-item>
          <el-descriptions-item label="频率">50Hz</el-descriptions-item>
          <el-descriptions-item label="驱动方式">LEDC PWM</el-descriptions-item>
        </el-descriptions>
      </div>

      <!-- 角度显示圆盘 -->
      <div class="angle-display">
        <div class="angle-circle">
          <svg viewBox="0 0 100 100" class="angle-gauge">
            <!-- 背景圆弧 -->
            <circle cx="50" cy="50" r="40" fill="none" stroke="#e4e7ed" stroke-width="8" stroke-linecap="round"
              :stroke-dasharray="backgroundArc" transform="rotate(135 50 50)" />
            <!-- 进度圆弧 -->
            <circle cx="50" cy="50" r="40" fill="none" stroke="#409eff" stroke-width="8" stroke-linecap="round"
              :stroke-dasharray="progressArc" transform="rotate(135 50 50)" />
          </svg>
          <div class="angle-value">
            <span class="value">{{ servoAngle }}</span>
            <span class="unit">°</span>
          </div>
        </div>
      </div>

      <!-- 角度滑块 -->
      <div class="slider-section">
        <el-slider v-model="servoAngle" :min="0" :max="180" :step="10" :show-tooltip="false"
          @input="isDragging.value = true" @change="handleServoChange" />
        <div class="slider-labels">
          <span>0°</span>
          <span>75°</span>
          <span>150°</span>
          <span>180°</span>
        </div>
      </div>

      <!-- 快速设置按钮 -->
      <div class="quick-buttons">
        <el-button size="default" @click="setAngle(0)">0°</el-button>
        <el-button size="default" @click="setAngle(40)">40°</el-button>
        <el-button size="default" @click="setAngle(75)">75°</el-button>
        <el-button size="default" @click="setAngle(120)">120°</el-button>
        <el-button size="default" @click="setAngle(150)">150°</el-button>
        <el-button size="default" @click="setAngle(150)">180°</el-button>
      </div>
    </div>
  </PageCard>
</template>

<script setup>
import { ref, watch, computed } from 'vue'
import { ElMessage } from 'element-plus'
import { Rank } from '@element-plus/icons-vue'
import { PageCard } from '@/components/common'
import { controlServo } from '@/api/esp32'

const props = defineProps({
  status: {
    type: Object,
    default: () => ({})
  }
})

const servoAngle = ref(90)
const isDragging = ref(false)

// SVG圆弧常量
const CIRCLE_RADIUS = 40
const CIRCLE_CIRCUMFERENCE = 2 * Math.PI * CIRCLE_RADIUS // 251.33
const MAX_ARC_LENGTH = 251.33

// 计算进度圆弧长度（0-150度对应0-270度圆弧）
const progressArc = computed(() => {
  const progress = servoAngle.value / 150
  const arcLength = progress * MAX_ARC_LENGTH * 0.75 // 150度对应270度圆弧
  return `${arcLength} ${MAX_ARC_LENGTH}`
})

// 背景圆弧（270度的弧度）
const backgroundArc = computed(() => {
  const bgLength = MAX_ARC_LENGTH * 0.75
  return `${bgLength} ${MAX_ARC_LENGTH}`
})

watch(() => props.status.servo_angle, (newVal) => {
  if (newVal !== undefined && !isDragging.value) {
    servoAngle.value = newVal
  }
}, { immediate: true })

const setAngle = (angle) => {
  servoAngle.value = angle
  controlServo({ angle })
}

const handleServoChange = async (angle) => {
  isDragging.value = false
  try {
    await controlServo({ angle })
  } catch (error) {
    ElMessage.error('控制失败')
  }
}
</script>

<style scoped lang="scss">
@import '@/styles/variables';
@import '@/styles/mixins';

.servo-card {
  width: 100%;
}

.control-content {
  padding: $spacing-md 0;
}

.pin-info {
  margin-bottom: $spacing-xl;
}

.angle-display {
  display: flex;
  justify-content: center;
  margin-bottom: $spacing-xl;
}

.angle-circle {
  position: relative;
  width: 160px;
  height: 160px;
}

.angle-gauge {
  width: 100%;
  height: 100%;
}

.angle-value {
  position: absolute;
  top: 50%;
  left: 50%;
  transform: translate(-50%, -50%);
  text-align: center;
}

.angle-value .value {
  display: block;
  font-size: 36px;
  font-weight: bold;
  color: $text-primary;
  line-height: 1;
}

.angle-value .unit {
  font-size: $font-size-md;
  color: $text-secondary;
}

.slider-section {
  padding: 0 $spacing-lg;
  margin-bottom: $spacing-lg;
}

.slider-labels {
  display: flex;
  justify-content: space-between;
  margin-top: $spacing-sm;
  font-size: $font-size-xs;
  color: $text-secondary;
}

.quick-buttons {
  display: flex;
  justify-content: center;
  gap: $spacing-sm;
  padding: 0 $spacing-lg;
}
</style>
