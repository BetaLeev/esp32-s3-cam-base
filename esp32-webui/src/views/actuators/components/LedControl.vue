<template>
  <el-card class="led-card">
    <template #header>
      <div class="card-header">
        <span>LED控制</span>
        <el-tag :type="localConfig.enabled ? 'success' : 'info'" size="small">
          {{ localConfig.enabled ? '运行中' : '停止' }}
        </el-tag>
      </div>
    </template>

    <el-form label-width="100px" :model="localConfig" label-position="left">
      <!-- 引脚选择 -->
      <el-form-item label="GPIO引脚">
        <el-select v-model="localConfig.pin" placeholder="选择引脚" style="width: 100%">
          <el-option
            v-for="pin in availablePins"
            :key="pin"
            :label="`GPIO ${pin}`"
            :value="pin"
            :disabled="usedPins.includes(pin)"
          />
        </el-select>
        <div class="form-tip" v-if="localConfig.pin && usedPins.includes(localConfig.pin)">
          <el-alert type="warning" :closable="false" show-icon>
            该引脚已被其他设备使用
          </el-alert>
        </div>
      </el-form-item>

      <!-- 触发模式 -->
      <el-form-item label="触发模式">
        <el-radio-group v-model="localConfig.trigger_mode">
          <el-radio value="static">静态</el-radio>
          <el-radio value="blink">闪烁</el-radio>
        </el-radio-group>
      </el-form-item>

      <!-- 静态模式: 初始电平 -->
      <el-form-item label="初始电平" v-if="localConfig.trigger_mode === 'static'">
        <el-radio-group v-model="localConfig.initial_level">
          <el-radio :value="1">高电平 (点亮)</el-radio>
          <el-radio :value="0">低电平 (熄灭)</el-radio>
        </el-radio-group>
      </el-form-item>

      <!-- 闪烁模式: 参数设置 -->
      <template v-if="localConfig.trigger_mode === 'blink'">
        <el-divider content-position="left">闪烁参数</el-divider>

        <el-form-item label="高电平时长">
          <div class="input-with-unit">
            <el-input-number v-model="localConfig.high_duration" :min="0" :max="60" :step="0.1" />
            <span class="unit">秒</span>
          </div>
        </el-form-item>

        <el-form-item label="低电平时长">
          <div class="input-with-unit">
            <el-input-number v-model="localConfig.low_duration" :min="0" :max="60" :step="0.1" />
            <span class="unit">秒</span>
          </div>
        </el-form-item>

        <el-form-item label="重复次数">
          <div class="repeat-config">
            <el-input-number v-model="localConfig.repeat_count" :min="1" :max="999" :disabled="localConfig.infinite" />
            <el-checkbox v-model="localConfig.infinite">无限循环</el-checkbox>
          </div>
        </el-form-item>

        <!-- 预览 -->
        <el-form-item label="预览">
          <div class="blink-preview">
            <span class="blink-high" :class="{ active: previewState }">高 {{ localConfig.high_duration }}s</span>
            <span class="blink-divider">→</span>
            <span class="blink-low" :class="{ active: !previewState }">低 {{ localConfig.low_duration }}s</span>
            <span class="blink-count" v-if="!localConfig.infinite">× {{ localConfig.repeat_count }}次</span>
            <span class="blink-count infinite" v-else>× ∞</span>
          </div>
        </el-form-item>
      </template>

      <!-- 操作按钮 -->
      <el-form-item>
        <el-button
          :type="localConfig.enabled ? 'danger' : 'primary'"
          @click="handleToggle"
          :loading="actionLoading"
          :disabled="!localConfig.pin && !localConfig.enabled"
        >
          <el-icon v-if="localConfig.enabled"><VideoPause /></el-icon>
          <el-icon v-else><VideoPlay /></el-icon>
          {{ localConfig.enabled ? '停止' : '启动' }}
        </el-button>
        <el-button @click="handleApply">应用配置</el-button>
      </el-form-item>
    </el-form>

    <!-- 状态显示 -->
    <el-divider content-position="left">当前状态</el-divider>
    <el-descriptions :column="2" border size="small">
      <el-descriptions-item label="当前电平">
        <el-tag :type="status.current_level ? 'success' : 'info'" size="small">
          {{ status.current_level ? '高' : '低' }}
        </el-tag>
      </el-descriptions-item>
      <el-descriptions-item label="已执行次数">
        {{ status.executed_count }} / {{ status.total_count || '∞' }}
      </el-descriptions-item>
      <el-descriptions-item label="已用时间">
        {{ status.elapsed_time || 0 }} 秒
      </el-descriptions-item>
      <el-descriptions-item label="剩余时间">
        {{ status.remaining_time || '∞' }} 秒
      </el-descriptions-item>
    </el-descriptions>
  </el-card>
</template>

<script setup>
import { ref, reactive, watch } from 'vue'
import { ElMessage } from 'element-plus'
import { VideoPlay, VideoPause } from '@element-plus/icons-vue'
import { controlLed } from '@/api/esp32'

const props = defineProps({
  status: {
    type: Object,
    default: () => ({})
  },
  usedPins: {
    type: Array,
    default: () => []
  }
})

const emit = defineEmits(['updateStatus'])

const actionLoading = ref(false)
const previewState = ref(true)

// 可用引脚 (排除系统占用和冲突引脚)
const availablePins = [12, 14, 15, 16, 22, 23, 25, 26, 27]

// 本地配置
const localConfig = reactive({
  pin: 2,
  trigger_mode: 'blink',
  initial_level: 1,
  high_duration: 1,
  low_duration: 1,
  repeat_count: 3,
  infinite: false,
  enabled: false
})

// 实际状态
const status = reactive({
  current_level: 0,
  executed_count: 0,
  total_count: 3,
  elapsed_time: 0,
  remaining_time: 0
})

// 监听 props.status 变化
watch(() => props.status, (newStatus) => {
  if (newStatus) {
    Object.assign(status, {
      current_level: newStatus.led_current_level ?? 0,
      executed_count: newStatus.led_executed_count ?? 0,
      total_count: newStatus.led_total_count ?? 3,
      elapsed_time: newStatus.led_elapsed_time ?? 0,
      remaining_time: newStatus.led_remaining_time ?? 0
    })
  }
}, { immediate: true })

// 监听配置变化，同步 enabled 状态
watch(() => localConfig.trigger_mode, () => {
  if (!localConfig.infinite) {
    localConfig.repeat_count = Math.max(1, localConfig.repeat_count)
  }
})

// 统一的切换按钮处理函数
const handleToggle = async () => {
  if (localConfig.enabled) {
    // 当前运行中，点击停止
    await handleStop()
  } else {
    // 当前停止，点击启动
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
    const config = {
      pin: localConfig.pin,
      action: 'start',
      trigger_mode: localConfig.trigger_mode,
      initial_level: localConfig.initial_level,
      high_duration: localConfig.high_duration,
      low_duration: localConfig.low_duration,
      repeat_count: localConfig.infinite ? -1 : localConfig.repeat_count
    }

    await controlLed(config)
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

const handleApply = async () => {
  if (!localConfig.pin) {
    ElMessage.warning('请选择引脚')
    return
  }

  actionLoading.value = true
  try {
    const config = {
      pin: localConfig.pin,
      action: 'config',
      trigger_mode: localConfig.trigger_mode,
      initial_level: localConfig.initial_level,
      high_duration: localConfig.high_duration,
      low_duration: localConfig.low_duration,
      repeat_count: localConfig.infinite ? -1 : localConfig.repeat_count
    }

    await controlLed(config)
    ElMessage.success('配置已应用')
  } catch (error) {
    ElMessage.error('配置失败')
  } finally {
    actionLoading.value = false
  }
}
</script>

<style scoped>
.led-card {
  margin-bottom: 20px;
}

.card-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
}

.form-tip {
  margin-top: 8px;
}

.input-with-unit {
  display: flex;
  align-items: center;
  gap: 8px;
}

.unit {
  color: #909399;
  font-size: 14px;
}

.repeat-config {
  display: flex;
  align-items: center;
  gap: 16px;
}

.blink-preview {
  display: flex;
  align-items: center;
  gap: 8px;
  padding: 8px 12px;
  background: #f5f7fa;
  border-radius: 4px;
  font-size: 13px;
}

.blink-high {
  padding: 2px 8px;
  background: #f4f4f5;
  border-radius: 4px;
  color: #909399;
}

.blink-high.active {
  background: #67c23a;
  color: #fff;
}

.blink-low {
  padding: 2px 8px;
  background: #f4f4f5;
  border-radius: 4px;
  color: #909399;
}

.blink-low.active {
  background: #909399;
  color: #fff;
}

.blink-divider {
  color: #409eff;
  font-weight: bold;
}

.blink-count {
  margin-left: 8px;
  color: #e6a23c;
  font-weight: 600;
}

.blink-count.infinite {
  color: #f56c6c;
}
</style>
