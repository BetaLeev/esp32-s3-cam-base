<template>
  <el-card class="pulse-card">
    <template #header>
      <div class="card-header">
        <span>脉冲控制</span>
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
      </el-form-item>

      <!-- 脉冲模式 -->
      <el-form-item label="脉冲模式">
        <el-radio-group v-model="localConfig.mode">
          <el-radio value="single">
            单次脉冲
            <el-tooltip content="触发动作，如电磁阀、继电器的一次性开关" placement="top">
              <el-icon class="mode-tip"><QuestionFilled /></el-icon>
            </el-tooltip>
          </el-radio>
          <el-radio value="continuous">
            连续脉冲
            <el-tooltip content="调速控制，通过占空比调节输出功率" placement="top">
              <el-icon class="mode-tip"><QuestionFilled /></el-icon>
            </el-tooltip>
          </el-radio>
        </el-radio-group>
      </el-form-item>

      <!-- 脉冲参数 -->
      <el-divider content-position="left">脉冲参数</el-divider>

      <el-form-item label="强度">
        <div class="slider-with-value">
          <el-slider v-model="localConfig.intensity" :min="0" :max="100" :step="1" />
          <span class="value-display">{{ localConfig.intensity }}%</span>
        </div>
        <div class="param-tip">控制PWM占空比，影响输出功率</div>
      </el-form-item>

      <el-form-item label="频率">
        <div class="input-with-suffix">
          <el-input-number v-model="localConfig.frequency" :min="1" :max="1000" :step="1" />
          <span class="suffix">Hz</span>
        </div>
        <div class="param-tip">每秒脉冲数量，影响响应速度</div>
      </el-form-item>

      <el-form-item label="脉冲宽度" v-if="localConfig.mode === 'single'">
        <div class="input-with-suffix">
          <el-input-number v-model="localConfig.pulse_width" :min="1" :max="1000" :step="1" />
          <span class="suffix">ms</span>
        </div>
        <div class="param-tip">高电平持续时间，控制脉冲能量</div>
      </el-form-item>
      <el-form-item v-else>
        <div class="param-tip info">
          <el-icon><InfoFilled /></el-icon>
          连续模式下脉冲宽度由强度自动计算
        </div>
      </el-form-item>

      <!-- 使用说明 -->
      <el-divider content-position="left">使用指南</el-divider>
      <div class="guide-section">
        <el-collapse>
          <el-collapse-item title="常见设备推荐参数" name="1">
            <el-table :data="guideData" size="small" border>
              <el-table-column prop="device" label="设备类型" width="100" />
              <el-table-column prop="frequency" label="频率范围" width="100" />
              <el-table-column prop="width" label="脉冲宽度" width="100" />
              <el-table-column prop="description" label="说明" />
            </el-table>
          </el-collapse-item>
          <el-collapse-item title="参数影响说明" name="2">
            <div class="guide-content">
              <p><strong>强度（0-100%）</strong>：控制输出电压的有效值，类似调速器的功率档位。100%时输出满幅电压。</p>
              <p><strong>频率（Hz）</strong>：每秒脉冲数。频率越高，电机运转越平稳；频率越低，电机可能有抖动。</p>
              <p><strong>脉冲宽度（ms）</strong>：每个脉冲的高电平持续时间。仅单次脉冲模式使用，影响脉冲能量。</p>
            </div>
          </el-collapse-item>
        </el-collapse>
      </div>

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
      </el-form-item>
    </el-form>

    <!-- 状态显示 -->
    <el-divider content-position="left">当前状态</el-divider>
    <el-descriptions :column="2" border size="small">
      <el-descriptions-item label="当前强度">
        <el-progress
          :percentage="status.current_intensity"
          :stroke-width="10"
          :show-text="true"
          style="width: 120px"
        />
      </el-descriptions-item>
      <el-descriptions-item label="已发送脉冲">
        {{ status.pulse_count }} 个
      </el-descriptions-item>
      <el-descriptions-item label="运行时间">
        {{ status.elapsed_time || 0 }} 秒
      </el-descriptions-item>
      <el-descriptions-item label="引脚状态">
        <el-tag :type="status.pin_level ? 'success' : 'info'" size="small">
          {{ status.pin_level ? '高' : '低' }}
        </el-tag>
      </el-descriptions-item>
    </el-descriptions>
  </el-card>
</template>

<script setup>
import { ref, reactive, watch } from 'vue'
import { ElMessage } from 'element-plus'
import { QuestionFilled, InfoFilled, VideoPlay, VideoPause } from '@element-plus/icons-vue'
import { controlPulse } from '@/api/esp32'

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

const actionLoading = ref(false)

// 可用引脚 (支持PWM的GPIO)
// 参考 board/index.vue 中定义的可用 GPIO，排除系统占用和冲突引脚
// 排除: 0(Strapping), 3(Strapping), 26-37(Flash/PSRAM), 39-44(SD/TF/UART)
// 排除: 1-2,4-20(摄像头), 40-42(电机), 48(舵机)
// 排除: 18-21(音频 I2S/GAIN), 19-20(USB)
const availablePins = [12, 13, 14, 15, 16, 22, 23, 25, 26, 27]

// 设备参数指南数据
const guideData = [
  { device: '直流电机', frequency: '500-1000 Hz', width: '—', description: '普通直流电机调速' },
  { device: '舵机', frequency: '50 Hz', width: '1-2 ms', description: 'SG90等小型舵机' },
  { device: '电磁阀', frequency: '1-100 Hz', width: '10-50 ms', description: '高速开关控制' },
  { device: '继电器', frequency: '1-10 Hz', width: '10-100 ms', description: '低频脉冲触发' },
  { device: 'LED调光', frequency: '1000+ Hz', width: '—', description: '无闪烁调光' },
  { device: '超声波', frequency: '40 kHz', width: '0.1 ms', description: '需外部电路配合' }
]

// 本地配置
const localConfig = reactive({
  pin: 2,
  mode: 'single',
  intensity: 50,
  frequency: 10,
  pulse_width: 100,
  enabled: false
})

// 实际状态
const status = reactive({
  current_intensity: 0,
  pulse_count: 0,
  elapsed_time: 0,
  pin_level: 0
})

// 监听 props.status 变化
watch(() => props.status, (newStatus) => {
  if (newStatus) {
    Object.assign(status, {
      current_intensity: newStatus.pulse_current_intensity ?? 0,
      pulse_count: newStatus.pulse_count ?? 0,
      elapsed_time: newStatus.pulse_elapsed_time ?? 0,
      pin_level: newStatus.pulse_pin_level ?? 0
    })
  }
}, { immediate: true })

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

<style scoped>
.pulse-card {
  margin-bottom: 20px;
}

.card-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
}

.slider-with-value {
  display: flex;
  align-items: center;
  width: 100%;
}

.slider-with-value .el-slider {
  flex: 1;
}

.value-display {
  min-width: 50px;
  text-align: right;
  font-weight: 600;
  color: #409eff;
  margin-left: 12px;
}

.input-with-suffix {
  display: flex;
  align-items: center;
  gap: 8px;
}

.suffix {
  color: #909399;
  font-size: 14px;
  min-width: 30px;
}

.form-tip {
  font-size: 12px;
  color: #909399;
  margin-top: 4px;
}

.param-tip {
  font-size: 12px;
  color: #67c23a;
  margin-top: 4px;
}

.guide-section {
  margin-bottom: 16px;
}

.guide-content p {
  margin: 8px 0;
  font-size: 13px;
  line-height: 1.6;
  color: #606266;
}

.guide-content strong {
  color: #409eff;
}

.mode-tip {
  margin-left: 4px;
  color: #909399;
  cursor: pointer;
}

.param-tip.info {
  display: flex;
  align-items: center;
  gap: 4px;
  color: #909399;
}
</style>
