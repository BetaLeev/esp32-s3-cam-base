<template>
  <div class="player-view">
    <el-card>
      <template #header>
        <div class="card-header">
          <span>{{ fileName }}</span>
          <el-button @click="goBack">
            <el-icon><Back /></el-icon> 返回
          </el-button>
        </div>
      </template>

      <div class="player-controls">
        <!-- 播放方式选择 -->
        <div class="play-mode-selector">
          <el-radio-group v-model="playMode" size="small">
            <el-radio-button value="browser">浏览器播放</el-radio-button>
            <el-radio-button value="esp32" :disabled="!isAudio || !isAudioFile">ESP32播放</el-radio-button>
          </el-radio-group>
          <span class="mode-hint" v-if="!isAudioFile && isAudio">（仅支持WAV/MP3格式）</span>
        </div>

        <!-- ESP32 音量控制 -->
        <div class="esp32-controls" v-if="playMode === 'esp32'">
          <div class="volume-control">
            <span class="volume-label">音量: {{ volume }}%</span>
            <el-slider
              v-model="volume"
              :min="0"
              :max="100"
              :show-tooltip="false"
              @change="handleVolumeChange"
            />
          </div>
          <div class="esp32-buttons">
            <el-button type="primary" @click="playOnEsp32" :loading="playing">
              <el-icon><VideoPlay /></el-icon> 播放
            </el-button>
            <el-button @click="stopOnEsp32">
              <el-icon><VideoPause /></el-icon> 停止
            </el-button>
          </div>
        </div>
      </div>

      <div class="player-container">
        <div v-if="loading" class="loading">
          <el-icon class="is-loading"><Loading /></el-icon>
          <span>{{ loadingText }}</span>
        </div>
        <div v-if="error" class="error">
          <el-icon><WarningFilled /></el-icon>
          <span>{{ error }}</span>
        </div>

        <video
          v-if="isVideo && playMode === 'browser' && !error && mediaSrc"
          ref="videoRef"
          :src="mediaSrc"
          controls
          autoplay
          @canplay="onCanPlay"
          @error="onError"
          @progress="onProgress"
        ></video>

        <audio
          v-if="isAudio && playMode === 'browser' && !error && mediaSrc"
          ref="audioRef"
          :src="mediaSrc"
          controls
          autoplay
          @canplay="onCanPlay"
          @error="onError"
          @progress="onProgress"
        ></audio>

        <!-- ESP32 播放状态 -->
        <div v-if="playMode === 'esp32' && playing" class="esp32-playing">
          <el-icon class="is-loading"><VideoPlay /></el-icon>
          <span>ESP32 正在播放...</span>
        </div>
      </div>
    </el-card>
  </div>
</template>

<script setup>
import { ref, computed, onMounted, onUnmounted } from 'vue'
import { useRouter, useRoute } from 'vue-router'
import { ElMessage } from 'element-plus'
import { Back, Loading, WarningFilled, VideoPlay, VideoPause } from '@element-plus/icons-vue'
import { playAudioFile, stopAudio, setAudioVolume, getAudioStatusSafe } from '@/api/esp32'

const router = useRouter()
const route = useRoute()

const videoRef = ref(null)
const audioRef = ref(null)
const loading = ref(true)
const loadingText = ref('加载中...')
const error = ref('')
const mediaSrc = ref('')
const playMode = ref('browser')
const volume = ref(80)
const playing = ref(false)

const fileName = route.query.name || '播放'
const mediaType = route.query.type || 'audio'
const filePath = route.query.path || ''
const isVideo = mediaType === 'video'
const isAudio = mediaType === 'audio'
const isWavFile = computed(() => fileName.toLowerCase().endsWith('.wav'))
const isMp3File = computed(() => fileName.toLowerCase().endsWith('.mp3'))
const isAudioFile = computed(() => isWavFile.value || isMp3File.value)

const loadMedia = async () => {
  loading.value = true
  loadingText.value = '加载中...'
  error.value = ''
  mediaSrc.value = ''

  try {
    if (!filePath) {
      throw new Error('没有文件路径')
    }

    const url = `/fs/files?path=${encodeURIComponent(filePath)}`

    console.log('加载媒体:', url)

    const response = await fetch(url)
    if (!response.ok) {
      throw new Error(`HTTP ${response.status}`)
    }

    const blob = await response.blob()
    const objectUrl = URL.createObjectURL(blob)
    mediaSrc.value = objectUrl
    loading.value = false
    loadingText.value = '加载完成'
  } catch (e) {
    console.error('加载失败:', e)
    error.value = '加载失败: ' + e.message
    loading.value = false
  }
}

const loadAudioStatus = async () => {
  const { data } = await getAudioStatusSafe()
  if (data) {
    volume.value = data.volume !== undefined ? data.volume : 80
  }
}

const playOnEsp32 = async () => {
  try {
    playing.value = true
    await setAudioVolume(volume.value)

    const res = await Promise.race([
      playAudioFile(filePath),
      new Promise((_, reject) =>
        setTimeout(() => reject(new Error('timeout')), 10000)
      )
    ])

    if (res.data.success) {
      ElMessage.success('ESP32 播放中: ' + fileName)
      setTimeout(() => {
        playing.value = false
      }, Math.min(res.data.duration_ms, 30000))
    } else {
      const errorMsg = res.data.error || res.data.message || '播放失败'
      if (errorMsg.includes('格式不支持')) {
        ElMessage.warning('MP3 格式暂不支持，请使用 WAV 格式')
      } else {
        ElMessage.error('播放失败: ' + errorMsg)
      }
      playing.value = false
    }
  } catch (e) {
    if (e.message === 'timeout') {
      ElMessage.warning('播放超时，MP3 格式暂不支持')
    } else {
      ElMessage.error('播放失败')
    }
    playing.value = false
  }
}

const stopOnEsp32 = async () => {
  try {
    await stopAudio()
    playing.value = false
    ElMessage.success('已停止')
  } catch (e) {
    console.error('停止失败:', e)
  }
}

const handleVolumeChange = async (val) => {
  try {
    await setAudioVolume(val)
  } catch (e) {
    console.error('设置音量失败:', e)
  }
}

const goBack = () => {
  if (videoRef.value) videoRef.value.pause()
  if (audioRef.value) audioRef.value.pause()
  router.back()
}

const onCanPlay = () => {
  loading.value = false
  loadingText.value = '加载完成'
}

const onProgress = (e) => {
  const target = e.target
  if (target.buffered.length > 0) {
    const percent = Math.round((target.buffered.end(0) / target.duration) * 100)
    loadingText.value = `缓冲中... ${percent}%`
  }
}

const onError = () => {
  loading.value = false
  error.value = '媒体加载失败'
}

const handleKeydown = (e) => {
  if (e.key === 'Escape') {
    goBack()
  }
}

onMounted(() => {
  document.addEventListener('keydown', handleKeydown)
  loadMedia()
  loadAudioStatus()
})

onUnmounted(() => {
  document.removeEventListener('keydown', handleKeydown)
  if (mediaSrc.value) {
    URL.revokeObjectURL(mediaSrc.value)
  }
  stopOnEsp32()
})
</script>

<style scoped>
.player-view {
  max-width: 900px;
  margin: 0 auto;
}

.card-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
}

.player-controls {
  margin-bottom: 16px;
}

.play-mode-selector {
  display: flex;
  align-items: center;
  gap: 12px;
  margin-bottom: 12px;
}

.mode-hint {
  color: #909399;
  font-size: 12px;
}

.esp32-controls {
  background: #f5f7fa;
  border-radius: 8px;
  padding: 16px;
}

.volume-control {
  margin-bottom: 12px;
}

.volume-label {
  display: block;
  color: #606266;
  font-size: 13px;
  margin-bottom: 8px;
}

.esp32-buttons {
  display: flex;
  gap: 8px;
}

.player-container {
  display: flex;
  justify-content: center;
  align-items: center;
  min-height: 300px;
  background: #000;
  border-radius: 8px;
  overflow: hidden;
  position: relative;
}

.player-container video {
  width: 100%;
  max-height: 60vh;
}

.player-container audio {
  width: 100%;
  max-width: 500px;
}

.loading, .error {
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 10px;
  color: #fff;
  position: absolute;
}

.error {
  color: #f56c6c;
}

.esp32-playing {
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 10px;
  color: #67c23a;
  font-size: 16px;
}
</style>
