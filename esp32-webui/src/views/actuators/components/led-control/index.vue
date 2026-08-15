<template>
  <PageCard title="LED控制" icon="Sunny" class="led-card" :loading="actionLoading">
    <template #header-extra>
      <StatusBadge :status="localConfig.enabled" :text="localConfig.enabled ? '运行中' : '停止'" />
    </template>

    <div class="control-content">
      <!-- GPIO选择 -->
      <div class="gpio-row">
        <span class="gpio-label">GPIO</span>
        <el-select v-model="localConfig.pin" size="default" placeholder="选择引脚" :disabled="localConfig.enabled"
          class="gpio-select">
          <el-option v-for="pin in availablePins" :key="pin" :label="`GPIO ${pin}`" :value="pin" />
        </el-select>
      </div>

      <!-- 模式选择 -->
      <div class="gpio-row">
        <span class="gpio-label">模式</span>
        <el-radio-group v-model="localConfig.trigger_mode" size="default" :disabled="localConfig.enabled">
          <el-radio-button value="static">静态</el-radio-button>
          <el-radio-button value="blink">闪烁</el-radio-button>
        </el-radio-group>
      </div>

      <!-- 闪烁参数 -->
      <template v-if="localConfig.trigger_mode === 'blink' && !localConfig.enabled">
        <el-divider content-position="left">闪烁参数</el-divider>
        <div class="blink-params">
          <div class="blink-row">
            <span class="blink-label">高电平</span>
            <el-input-number v-model="localConfig.high_duration" size="default" :min="0.1" :max="60" :step="0.1"
              controls-position="right" />
            <span class="blink-unit">秒</span>
          </div>
          <div class="blink-row">
            <span class="blink-label">低电平</span>
            <el-input-number v-model="localConfig.low_duration" size="default" :min="0.1" :max="60" :step="0.1"
              controls-position="right" />
            <span class="blink-unit">秒</span>
          </div>
          <div class="blink-row">
            <span class="blink-label">重复</span>
            <el-input-number v-model="localConfig.repeat_count" size="default" :min="1" :max="999"
              :disabled="localConfig.infinite" controls-position="right" />
            <el-checkbox v-model="localConfig.infinite" size="default">无限</el-checkbox>
          </div>
        </div>
      </template>

      <!-- 操作按钮 -->
      <div class="action-buttons">
        <el-button :type="localConfig.enabled ? 'danger' : 'primary'" @click="handleToggle"
          :disabled="!localConfig.pin && !localConfig.enabled" size="default">
          <el-icon v-if="localConfig.enabled">
            <VideoPause />
          </el-icon>
          <el-icon v-else>
            <VideoPlay />
          </el-icon>
          {{ localConfig.enabled ? '停止' : '启动' }}
        </el-button>
      </div>
    </div>
  </PageCard>
</template>

<script setup>
import { ref, reactive, watch } from 'vue'
import { ElMessage } from 'element-plus'
import { VideoPlay, VideoPause, Sunny } from '@element-plus/icons-vue'
import { PageCard, StatusBadge } from '@/components/common'
import { controlLed } from '@/api/esp32'

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
  pin: null,
  trigger_mode: 'blink',
  initial_level: 1,
  high_duration: 1,
  low_duration: 1,
  repeat_count: 3,
  infinite: false,
  enabled: false
})

watch(() => props.status, (newStatus) => {
  if (newStatus) {
    localConfig.enabled = newStatus.led_enabled ?? false
    localConfig.pin = newStatus.led_pin ?? null
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
    await controlLed({
      pin: localConfig.pin,
      action: 'start',
      trigger_mode: localConfig.trigger_mode,
      initial_level: localConfig.initial_level,
      high_duration: localConfig.high_duration,
      low_duration: localConfig.low_duration,
      repeat_count: localConfig.infinite ? -1 : localConfig.repeat_count
    })
    localConfig.enabled = true
    ElMessage.success('LED控制已启动')
  } catch (error) {
    ElMessage.error('启动失败')
  } finally {
    actionLoading.value = false
  }
}

const handleStop = async () => {
  actionLoading.value = true
  try {
    await controlLed({ action: 'stop' })
    localConfig.enabled = false
    ElMessage.success('LED控制已停止')
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

.blink-params {
  display: flex;
  flex-direction: column;
  gap: $spacing-sm;
}

.blink-row {
  display: flex;
  align-items: center;
  gap: $spacing-sm;
}

.blink-label {
  width: 60px;
  font-size: $font-size-sm;
  color: $text-secondary;
}

.blink-unit {
  font-size: $font-size-sm;
  color: $text-secondary;
}

.action-buttons {
  display: flex;
  justify-content: center;
  margin-top: $spacing-xl;
}
</style>
