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
          v-if="isVideo && !error && mediaSrc"
          ref="videoRef"
          :src="mediaSrc"
          controls
          autoplay
          @canplay="onCanPlay"
          @error="onError"
          @progress="onProgress"
        ></video>

        <audio
          v-if="isAudio && !error && mediaSrc"
          ref="audioRef"
          :src="mediaSrc"
          controls
          autoplay
          @canplay="onCanPlay"
          @error="onError"
          @progress="onProgress"
        ></audio>
      </div>
    </el-card>
  </div>
</template>

<script setup>
import { ref, onMounted, onUnmounted } from 'vue'
import { useRouter, useRoute } from 'vue-router'
import { Back, Loading, WarningFilled } from '@element-plus/icons-vue'

const router = useRouter()
const route = useRoute()

const videoRef = ref(null)
const audioRef = ref(null)
const loading = ref(true)
const loadingText = ref('加载中...')
const error = ref('')
const mediaSrc = ref('')

const fileName = route.query.name || '播放'
const mediaType = route.query.type || 'audio'
const isVideo = mediaType === 'video'
const isAudio = mediaType === 'audio'

const loadMedia = async () => {
  loading.value = true
  loadingText.value = '加载中...'
  error.value = ''
  mediaSrc.value = ''

  try {
    const path = route.query.path
    if (!path) {
      throw new Error('没有文件路径')
    }

    // 使用相对路径，通过Vite代理转发到ESP32
    const url = `/fs/files?path=${encodeURIComponent(path)}`

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
})

onUnmounted(() => {
  document.removeEventListener('keydown', handleKeydown)
  if (mediaSrc.value) {
    URL.revokeObjectURL(mediaSrc.value)
  }
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

.player-container {
  display: flex;
  justify-content: center;
  align-items: center;
  min-height: 300px;
  background: #000;
  border-radius: 8px;
  overflow: hidden;
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
</style>
