<template>
  <div class="audio-control-card">
    <div class="card-header">
      <div class="header-left">
        <el-icon class="card-icon"><Microphone /></el-icon>
        <span class="card-title">音频控制</span>
      </div>
      <el-tag :type="statusTagType" size="small">{{ statusText }}</el-tag>
    </div>

    <div class="card-body">
      <!-- 状态信息 -->
      <div class="status-row">
        <span class="status-label">增益:</span>
        <span class="status-value">{{ audioStatus.gain_db }} dB</span>
      </div>

      <!-- 控制按钮 -->
      <div class="control-buttons">
        <el-button type="primary" @click="handlePlayTest" :loading="playing" :disabled="!audioStatus.initialized">
          <el-icon class="el-icon--left"><VideoPlay /></el-icon>
          播放测试音调
        </el-button>
        <el-button @click="handleStop" :disabled="!audioStatus.initialized || audioStatus.state !== 'playing'">
          <el-icon class="el-icon--left"><VideoPause /></el-icon>
          停止
        </el-button>
      </div>

      <!-- 增益调节 -->
      <div class="gain-control">
        <span class="gain-label">增益调节:</span>
        <div class="gain-buttons">
          <el-button
            v-for="gain in gainOptions"
            :key="gain"
            :type="audioStatus.gain_db === gain ? 'primary' : 'default'"
            size="small"
            @click="handleSetGain(gain)"
            :disabled="!audioStatus.initialized"
          >
            {{ gain }}dB
          </el-button>
        </div>
      </div>

      <!-- 音量滑块 -->
      <div class="volume-control">
        <span class="volume-label">音量: {{ audioStatus.volume }}%</span>
        <el-slider
          v-model="audioStatus.volume"
          :min="0"
          :max="100"
          :disabled="!audioStatus.initialized"
          @change="handleSetVolume"
          :show-tooltip="false"
        />
      </div>
    </div>
  </div>
</template>

<script setup>
import { reactive, ref, computed, onMounted, onUnmounted } from 'vue'
import { ElMessage } from 'element-plus'
import { Microphone, VideoPlay, VideoPause } from '@element-plus/icons-vue'
import { getAudioStatusSafe, playAudioTest, stopAudio, setAudioGain, setAudioVolume } from '@/api/esp32'

const audioStatus = reactive({
  initialized: false,
  state: 'uninit',
  gain: 2,
  gain_db: 9,
  volume: 80
})

const playing = ref(false)
const gainOptions = [3, 6, 9, 12]
let timer = null

const statusText = computed(() => {
  const stateMap = {
    'uninit': '未初始化',
    'ready': '就绪',
    'playing': '播放中',
    'paused': '已暂停',
    'error': '错误',
    'unknown': '未知'
  }
  return stateMap[audioStatus.state] || '未知'
})

const statusTagType = computed(() => {
  const typeMap = {
    'uninit': 'info',
    'ready': 'success',
    'playing': 'warning',
    'paused': 'info',
    'error': 'danger'
  }
  return typeMap[audioStatus.state] || 'info'
})

const fetchAudioStatus = async () => {
  const { data } = await getAudioStatusSafe()
  if (data) {
    audioStatus.initialized = data.initialized
    audioStatus.state = data.state
    audioStatus.gain = data.gain
    audioStatus.gain_db = data.gain_db
    audioStatus.volume = data.volume !== undefined ? data.volume : 80
  }
}

const handlePlayTest = async () => {
  try {
    playing.value = true
    await playAudioTest()
    ElMessage.success('正在播放测试音调')
    setTimeout(() => {
      playing.value = false
      fetchAudioStatus()
    }, 2000)
  } catch (error) {
    playing.value = false
    ElMessage.error('播放失败')
  }
}

const handleStop = async () => {
  try {
    await stopAudio()
    ElMessage.success('已停止')
    fetchAudioStatus()
  } catch (error) {
    ElMessage.error('停止失败')
  }
}

const handleSetGain = async (gain) => {
  try {
    await setAudioGain(gain)
    audioStatus.gain_db = gain
    ElMessage.success(`增益已设置为 ${gain} dB`)
  } catch (error) {
    ElMessage.error('设置增益失败')
  }
}

const handleSetVolume = async (volume) => {
  try {
    await setAudioVolume(volume)
    audioStatus.volume = volume
  } catch (error) {
    ElMessage.error('设置音量失败')
  }
}

onMounted(() => {
  fetchAudioStatus()
  timer = setInterval(fetchAudioStatus, 5000)
})

onUnmounted(() => {
  if (timer) clearInterval(timer)
})
</script>

<style scoped lang="scss">
@import '@/styles/variables';

.audio-control-card {
  background: #fff;
  border-radius: $border-radius-base;
  box-shadow: $shadow-base;
  overflow: hidden;
}

.card-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding: $spacing-md $spacing-lg;
  background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
  color: #fff;
}

.header-left {
  display: flex;
  align-items: center;
  gap: $spacing-sm;
}

.card-icon {
  font-size: 20px;
}

.card-title {
  font-size: $font-size-base;
  font-weight: 600;
}

.card-body {
  padding: $spacing-lg;
}

.status-row {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin-bottom: $spacing-md;
  padding: $spacing-sm 0;
  border-bottom: 1px solid #eee;
}

.status-label {
  color: #666;
  font-size: $font-size-sm;
}

.status-value {
  font-size: $font-size-lg;
  font-weight: 600;
  color: $primary-color;
}

.control-buttons {
  display: flex;
  gap: $spacing-sm;
  margin-bottom: $spacing-md;

  .el-button {
    flex: 1;
  }
}

.gain-control {
  .gain-label {
    display: block;
    color: #666;
    font-size: $font-size-sm;
    margin-bottom: $spacing-sm;
  }

  .gain-buttons {
    display: flex;
    gap: $spacing-sm;

    .el-button {
      flex: 1;
    }
  }
}

.volume-control {
  margin-top: $spacing-md;
  padding-top: $spacing-md;
  border-top: 1px solid #eee;

  .volume-label {
    display: block;
    color: #666;
    font-size: $font-size-sm;
    margin-bottom: $spacing-sm;
  }
}
</style>
