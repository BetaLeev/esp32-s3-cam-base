<template>
  <PageCard title="脉冲控制" icon="Odometer" class="pulse-card" :loading="actionLoading">
    <template #header-extra>
      <StatusBadge :status="localConfig.enabled" :text="localConfig.enabled ? '运行中' : '停止'" />
    </template>

    <div class="control-content">
      <!-- GPIO选择 -->
      <div class="gpio-row">
        <span class="gpio-label">GPIO</span>
        <el-select v-model="localConfig.pin" size="default" placeholder="选择引脚" :disabled="localConfig.enabled" class="gpio-select">
          <el-option
            v-for="pin in availablePins"
            :key="pin"
            :label="`GPIO ${pin}`"
            :value="pin"
          />
        </el-select>
      </div>

      <!-- 模式选择 -->
      <div class="gpio-row">
        <span class="gpio-label">模式</span>
        <el-radio-group v-model="localConfig.mode" size="default" :disabled="localConfig.enabled">
          <el-radio-button value="single">单次</el-radio-button>
          <el-radio-button value="continuous">连续</el-radio-button>
        </el-radio-group>
      </div>

      <!-- 强度滑块 -->
      <div class="intensity-section">
        <div class="intensity-header">
          <span class="intensity-label">强度</span>
          <span class="intensity-value">{{ localConfig.intensity }}%</span>
        </div>
        <el-slider v-model="localConfig.intensity" :min="0" :max="100" :step="1" :disabled="localConfig.enabled" />
      </div>

      <!-- 参数配置 -->
      <div class="param-row">
        <span class="gpio-label">频率</span>
        <el-input-number v-model="localConfig.frequency" size="default" :min="100" :max="2000" :step="100" controls-position="right" :disabled="localConfig.enabled" />
        <span class="param-unit">Hz</span>
      </div>

      <div class="param-row">
        <span class="gpio-label">宽度</span>
        <el-input-number v-model="localConfig.pulse_width" size="default" :min="1" :max="1000" controls-position="right" :disabled="localConfig.enabled" />
        <span class="param-unit">ms</span>
      </div>

      <!-- 操作按钮 -->
      <div class="action-buttons">
        <el-button
          :type="localConfig.enabled ? 'danger' : 'primary'"
          @click="handleToggle"
          size="default"
        >
          <el-icon v-if="localConfig.enabled"><VideoPause /></el-icon>
          <el-icon v-else><VideoPlay /></el-icon>
          {{ localConfig.enabled ? '停止' : '启动' }}
        </el-button>
      </div>
    </div>
  </PageCard>
</template>

<script setup>
import { ref, reactive, watch } from 'vue'
import { ElMessage } from 'element-plus'
import { VideoPlay, VideoPause, Odometer } from '@element-plus/icons-vue'
import { PageCard, StatusBadge } from '@/components/common'
import { controlPulse } from '@/api/esp32'

const props = defineProps({
  status: {
    type: Object,
    default: () => ({})
  }
})

// 可用GPIO列表
const availablePins = [0, 19, 20, 21, 22]

const actionLoading = ref(false)

const localConfig = reactive({
  pin: undefined,
  mode: 'single',
  intensity: 50,
  frequency: 200,
  pulse_width: 100,
  enabled: false
})

watch(() => props.status, (newStatus) => {
  if (newStatus) {
    localConfig.enabled = newStatus.pulse_enabled ?? false
    if (newStatus.pulse_pin !== undefined) {
      localConfig.pin = newStatus.pulse_pin
    } else {
      localConfig.pin = undefined
    }
  }
}, { immediate: true })

const handleToggle = async () => {
  if (localConfig.enabled) {
    await handleStop()
  } else {
    await handleStart()
  }
}

const handleStart = async () => {
  if (!localConfig.pin) {
    ElMessage.warning('请选择引脚')
    return
  }
  actionLoading.value = true
  try {
    await controlPulse({
      pin: localConfig.pin,
      action: 'start',
      mode: localConfig.mode,
      intensity: localConfig.intensity,
      frequency: localConfig.frequency,
      pulse_width: localConfig.pulse_width
    })
    localConfig.enabled = true
    ElMessage.success('脉冲控制已启动')
  } catch (error) {
    ElMessage.error('启动失败')
  } finally {
    actionLoading.value = false
  }
}

const handleStop = async () => {
  actionLoading.value = true
  try {
    await controlPulse({ action: 'stop' })
    localConfig.enabled = false
    ElMessage.success('脉冲控制已停止')
  } catch (error) {
    ElMessage.error('停止失败')
  } finally {
    actionLoading.value = false
  }
}
</script>

<style scoped lang="scss">
@import '@/styles/variables';
@import '@/styles/mixins';

.gpio-row {
  @include gpio-select;
  margin-bottom: $spacing-md;
}

.intensity-section {
  margin-bottom: $spacing-lg;
  padding: $spacing-md;
  background: $bg-color;
  border-radius: $border-radius-base;
}

.intensity-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin-bottom: $spacing-sm;
}

.intensity-label {
  font-size: $font-size-sm;
  color: $text-secondary;
}

.intensity-value {
  font-size: $font-size-lg;
  font-weight: 600;
  color: $primary-color;
}

.param-row {
  display: flex;
  align-items: center;
  gap: $spacing-sm;
  margin-bottom: $spacing-md;
}

.param-unit {
  font-size: $font-size-sm;
  color: $text-secondary;
  min-width: 30px;
}

.action-buttons {
  display: flex;
  justify-content: center;
  margin-top: $spacing-xl;
}
</style>
