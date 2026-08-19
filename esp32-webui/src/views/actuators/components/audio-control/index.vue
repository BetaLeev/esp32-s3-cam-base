<template>
  <div class="audio-control-card">
    <!-- 卡片头部 -->
    <div class="card-header">
      <div class="header-left">
        <el-icon class="card-icon">
          <Microphone />
        </el-icon>
        <span class="card-title">音频与麦克风控制</span>
      </div>
      <div class="header-tags">
        <el-tag :type="audioStatusTagType" size="small">放音: {{ audioStatusText }}</el-tag>
        <el-tag :type="micStatusTagType" size="small" class="ml-1">麦克风: {{ micStatusText }}</el-tag>
      </div>
    </div>

    <div class="card-body">
      <!-- 1. 音频播放 (Audio) 独立控制区 -->
      <div class="section-title">
        <el-icon>
          <Headset />
        </el-icon>
        <span>扬声器播放 (Audio)</span>
      </div>

      <div class="status-row">
        <span class="status-label">增益:</span>
        <span class="status-value">{{ audioStatus.gain_db }} dB</span>
      </div>

      <div class="control-buttons">
        <el-button type="primary" @click="handlePlayTest" :loading="playing">
          <el-icon class="el-icon--left">
            <VideoPlay />
          </el-icon>
          播放测试音调
        </el-button>
        <el-button @click="handleStop" :disabled="!audioStatus.initialized || audioStatus.state !== 'playing'">
          <el-icon class="el-icon--left">
            <VideoPause />
          </el-icon>
          停止播放
        </el-button>
      </div>

      <!-- 增益调节 -->
      <div class="gain-control">
        <span class="gain-label">增益调节:</span>
        <div class="gain-buttons">
          <el-button v-for="gain in gainOptions" :key="gain"
            :type="audioStatus.gain_db === gain ? 'primary' : 'default'" size="small" @click="handleSetGain(gain)">
            {{ gain }}dB
          </el-button>
        </div>
      </div>

      <!-- 音量滑块 -->
      <div class="volume-control">
        <span class="volume-label">音量: {{ audioStatus.volume }}%</span>
        <el-slider v-model="audioStatus.volume" :min="0" :max="100" @change="handleSetVolume" :show-tooltip="false" />
      </div>

      <el-divider />

      <!-- 2. 麦克风 (Mic) 控制区 -->
      <div class="section-title">
        <el-icon>
          <Microphone />
        </el-icon>
        <span>麦克风测试与实时扩音</span>
      </div>

      <!-- 实时音量强度条 -->
      <div class="mic-progress-wrapper">
        <el-progress :percentage="Math.min(micStatus.sound_level, 100)" :color="micProgressColors" :stroke-width="10"
          :show-text="false" />
      </div>

      <!-- 实时扩音开关 -->
      <div class="status-row loopback-row">
        <span class="status-label">麦克风实时扩音 (Mic -> Speaker):</span>
        <el-switch v-model="micStatus.loopback" @change="handleToggleLoopback" active-text="开启" inactive-text="关闭" />
      </div>

      <!-- 3. 动态调参面板 -->
      <el-collapse class="mb-3">
        <el-collapse-item title="高级动态参数设置 (实时调优防杂音)" name="config">
          <div class="config-panel">
            <div class="config-item">
              <label>采样率 (Hz):</label>
              <el-input-number v-model="micConfig.sample_rate" :step="8000" :min="8000" :max="48000" size="small" />
            </div>
            <div class="config-item">
              <label>右移位数 (降低底噪/破音):</label>
              <el-slider v-model="micConfig.shift_bits" :min="0" :max="16" show-input size="small" />
            </div>
            <div class="config-item">
              <label>扩音缩放比例:</label>
              <el-slider v-model="micConfig.volume_scale" :min="0" :max="2" :step="0.1" show-input size="small" />
            </div>
            <el-button type="success" size="small" @click="handleSaveMicConfig" style="width: 100%; margin-top: 10px;">
              应用新参数
            </el-button>
          </div>
        </el-collapse-item>
      </el-collapse>

      <div class="control-buttons mt-3">
        <el-button v-if="!micStatus.testing" type="success" @click="handleStartMicTest" :loading="micLoading">
          <el-icon class="el-icon--left">
            <Microphone />
          </el-icon>
          开启麦克风测试
        </el-button>
        <el-button v-else type="danger" @click="handleStopMicTest" :loading="micLoading">
          <el-icon class="el-icon--left">
            <Mute />
          </el-icon>
          关闭麦克风测试
        </el-button>
      </div>

      <el-divider />

      <!-- 4. 语音识别显示区 -->
      <div class="section-title">
        <el-icon>
          <ChatDotRound />
        </el-icon>
        <span>语音识别结果</span>
      </div>

      <div class="speech-box">
        <div class="speech-item">
          <span class="speech-label">浏览器识别：</span>
          <span class="speech-text">{{ browserSpeechText || '暂无识别结果' }}</span>
        </div>
        <div class="speech-item">
          <span class="speech-label">设备命令词：</span>
          <span class="speech-text">{{ deviceCommandText || '暂无命令词' }}</span>
        </div>
      </div>

      <div class="speech-actions">
        <el-button :type="isListening ? 'danger' : 'primary'" size="small" @click="toggleSpeechRecognition">
          {{ isListening ? '停止浏览器识别' : '开始浏览器识别' }}
        </el-button>
      </div>
    </div>
  </div>
</template>

<script setup>
import { reactive, ref, computed, onMounted, onUnmounted } from 'vue'
import { ElMessage } from 'element-plus'
import { Microphone, Headset, VideoPlay, VideoPause, Mute, ChatDotRound } from '@element-plus/icons-vue'
import {
  getAudioStatusSafe, playAudioTest, stopAudio, setAudioGain, setAudioVolume,
  getMicStatusSafe, startMicTest, stopMicTest, getMicConfig, setMicConfig, getDeviceCommand
} from '@/api/esp32'

// ==================== 状态 ====================
const audioStatus = reactive({ initialized: false, state: 'uninit', gain: 2, gain_db: 9, volume: 80 })
const playing = ref(false)
const gainOptions = [3, 6, 9, 12]

const micStatus = reactive({ initialized: false, testing: false, sound_level: 0, loopback: false })
const micLoading = ref(false)
const micConfig = reactive({ sample_rate: 16000, shift_bits: 15, volume_scale: 0.2 })

// 语音识别相关
const browserSpeechText = ref('')
const deviceCommandText = ref('')
const isListening = ref(false)
let recognition = null

// ==================== 计算属性 ====================
const audioStatusText = computed(() => {
  const stateMap = { 'uninit': '未初始化', 'ready': '就绪', 'playing': '播放中', 'paused': '已暂停', 'error': '错误' }
  return stateMap[audioStatus.state] || '未知'
})

const audioStatusTagType = computed(() => {
  const typeMap = { 'uninit': 'info', 'ready': 'success', 'playing': 'warning', 'paused': 'info', 'error': 'danger' }
  return typeMap[audioStatus.state] || 'info'
})

const micStatusText = computed(() => !micStatus.initialized ? '未就绪' : (micStatus.testing ? '检测中' : '空闲'))
const micStatusTagType = computed(() => !micStatus.initialized ? 'info' : (micStatus.testing ? 'success' : 'info'))
const micProgressColors = [{ color: '#67c23a', percentage: 100 }]

// ==================== 逻辑 ====================
const fetchAudioStatus = async () => {
  const { data } = await getAudioStatusSafe()
  if (data) Object.assign(audioStatus, data)
}

const fetchMicStatus = async () => {
  const { data } = await getMicStatusSafe()
  if (data) Object.assign(micStatus, data)
}

const loadMicConfig = async () => {
  const { data } = await getMicConfig()
  if (data) Object.assign(micConfig, data)
}

const handleSaveMicConfig = async () => {
  try {
    await setMicConfig(micConfig)
    ElMessage.success('麦克风参数已动态更新！')
  } catch (e) {
    ElMessage.error('参数更新失败')
  }
}

const handlePlayTest = async () => {
  playing.value = true
  await playAudioTest()
  setTimeout(() => { playing.value = false; fetchAudioStatus() }, 2000)
}

const handleStop = async () => { await stopAudio(); fetchAudioStatus() }
const handleSetGain = async (gain) => { await setAudioGain(gain); audioStatus.gain_db = gain }
const handleSetVolume = async (vol) => { await setAudioVolume(vol) }

const handleStartMicTest = async () => { micLoading.value = true; await startMicTest(); micStatus.testing = true; micLoading.value = false }
const handleStopMicTest = async () => { micLoading.value = true; await stopMicTest(); micStatus.testing = false; micLoading.value = false }
const handleToggleLoopback = (val) => ElMessage.info(val ? '麦克风实时扩音已开启' : '麦克风实时扩音已关闭')

// ==================== 浏览器 Web Speech API ====================
const initSpeechRecognition = () => {
  const SpeechRecognition = window.SpeechRecognition || window.webkitSpeechRecognition
  if (!SpeechRecognition) {
    ElMessage.warning('当前浏览器不支持 Web Speech API')
    return
  }
  recognition = new SpeechRecognition()
  recognition.lang = 'zh-CN'
  recognition.continuous = true
  recognition.interimResults = true

  recognition.onresult = (event) => {
    let finalText = ''
    let interimText = ''
    for (let i = event.resultIndex; i < event.results.length; i++) {
      const result = event.results[i]
      if (result.isFinal) {
        finalText += result[0].transcript
      } else {
        interimText += result[0].transcript
      }
    }
    if (finalText) {
      browserSpeechText.value = finalText
      // TODO: 后端实现 /api/speech/text 后，可在这里 POST 到 ESP32 日志
    }
  }

  recognition.onend = () => {
    isListening.value = false
  }

  recognition.onerror = (event) => {
    console.error('Speech recognition error:', event.error)
    isListening.value = false
  }
}

const toggleSpeechRecognition = () => {
  if (!recognition) {
    initSpeechRecognition()
  }
  if (!recognition) return

  if (isListening.value) {
    recognition.stop()
    isListening.value = false
  } else {
    recognition.start()
    isListening.value = true
    browserSpeechText.value = ''
  }
}

// ==================== 定时获取设备命令词（预留） ====================
const fetchDeviceCommandText = async () => {
  try {
    const { data } = await getDeviceCommand()  // 需要你新增 API 方法
    if (data && data.text) {
      deviceCommandText.value = data.text
    }
  } catch (e) {
    console.error('获取设备命令词失败', e)
  }
}

let timer = null
onMounted(() => {
  fetchAudioStatus()
  fetchMicStatus()
  loadMicConfig()
  initSpeechRecognition()
  timer = setInterval(() => {
    fetchAudioStatus()
    fetchMicStatus()
    fetchDeviceCommandText()
  }, 2000)
})

onUnmounted(() => {
  clearInterval(timer)
  if (recognition && isListening.value) {
    recognition.stop()
  }
})
</script>

<style scoped lang="scss">
@import '@/styles/variables';

.audio-control-card {
  background: #fff;
  border-radius: 8px;
  box-shadow: 0 2px 12px rgba(0, 0, 0, 0.1);
  overflow: hidden;
}

.card-header {
  padding: 15px;
  background: linear-gradient(135deg, #667eea, #764ba2);
  color: #fff;
  display: flex;
  justify-content: space-between;
  align-items: center;
}

.card-title {
  font-weight: 600;
}

.card-body {
  padding: 20px;
}

.section-title {
  display: flex;
  align-items: center;
  gap: 6px;
  font-weight: 600;
  margin-bottom: 10px;
  color: #333;
}

.status-row {
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding: 8px 0;
  border-bottom: 1px solid #eee;
}

.loopback-row {
  background-color: #f9f9fb;
  border: none;
  border-radius: 4px;
  padding: 8px;
}

.config-panel {
  padding: 10px;
  background: #f0f2f5;
  border-radius: 4px;
}

.config-item {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin-bottom: 10px;
  font-size: 13px;
  color: #666;
}

.mt-3 {
  margin-top: 15px;
}

.ml-1 {
  margin-left: 4px;
}

.speech-box {
  background: #f8f9fa;
  border-radius: 4px;
  padding: 10px;
  margin-bottom: 10px;
}

.speech-item {
  display: flex;
  margin-bottom: 6px;
  font-size: 14px;
}

.speech-label {
  color: #666;
  min-width: 90px;
}

.speech-text {
  color: #333;
  font-weight: 500;
  word-break: break-all;
}

.speech-actions {
  display: flex;
  justify-content: flex-end;
}
</style>