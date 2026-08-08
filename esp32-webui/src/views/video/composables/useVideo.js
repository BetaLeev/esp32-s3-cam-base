import { ref, reactive, onMounted, onUnmounted, computed } from 'vue'
import { ElMessage } from 'element-plus'
import {
  getVideoInfo,
  getVideoStatus,
  getVideoFramesizes,
  getVideoStreamUrl,
  getVideoSnapshotUrl,
  setVideoConfig,
  startVideoStream,
  stopVideoStream
} from '@/api/esp32'

/**
 * @brief 视频模块业务逻辑 composable
 * 集中管理视频相关的响应式状态、API 调用、定时器与 FPS 计数，
 * 供 video/index.vue 及其子组件复用。
 */
export function useVideo() {
  // ========================================
  // 状态
  // ========================================

  const loading = ref(false)
  const actionLoading = ref('') // 'start' | 'stop' | 'config' | ''
  const isStreaming = ref(false)
  const fps = ref(0)
  const snapshotKey = ref(0)
  const paramsChanged = ref(false)

  // 原始信息（来自后端）
  const info = reactive({
    initialized: false,
    streaming: false,
    sensor: '',
    resolution: { width: 0, height: 0, framesize: 0 },
    params: {
      brightness: 0,
      contrast: 0,
      saturation: 0,
      gain: 0,
      exposure: 0,
      hmirror: false,
      vflip: false,
      quality: 12
    }
  })

  // 用户可编辑参数（双向绑定）
  const params = reactive({
    brightness: 0,
    contrast: 0,
    saturation: 0,
    gain: 0,
    exposure: 0,
    hmirror: false,
    vflip: false,
    framesize: 8, // 默认 SVGA (800x600)
    quality: 12
  })

  // 分辨率列表
  const framesizeList = ref([])

  // 定时器
  let statusTimer = null
  let fpsTimer = null
  let fpsFrameCount = 0

  // ========================================
  // URL 计算
  // ========================================

  const streamUrl = computed(() => getVideoStreamUrl())
  const snapshotUrl = computed(() => {
    // 依赖 snapshotKey，每次点击抓拍都会更新
    return `${getVideoSnapshotUrl()}&k=${snapshotKey.value}`
  })

  // ========================================
  // 数据获取
  // ========================================

  // 将后端信息同步到可编辑参数（仅当未处于"已修改"状态）
  const syncInfoToParams = () => {
    params.brightness = info.params.brightness
    params.contrast = info.params.contrast
    params.saturation = info.params.saturation
    params.hmirror = info.params.hmirror
    params.vflip = info.params.vflip
    params.framesize = info.resolution.framesize || 8
    params.quality = info.params.quality
    paramsChanged.value = false
  }

  const refreshInfo = async () => {
    loading.value = true
    try {
      const res = await getVideoInfo()
      // 统一响应格式: { status, code, message, data }
      const payload = res.data?.data
      if (payload) {
        info.initialized = !!payload.initialized
        info.streaming = !!payload.streaming
        info.sensor = payload.sensor || ''
        if (payload.resolution) {
          info.resolution.width = payload.resolution.width || 0
          info.resolution.height = payload.resolution.height || 0
          info.resolution.framesize = payload.resolution.framesize || 0
        }
        if (payload.params) {
          info.params.brightness = payload.params.brightness ?? 0
          info.params.contrast = payload.params.contrast ?? 0
          info.params.saturation = payload.params.saturation ?? 0
          info.params.gain = payload.params.gain ?? 0
          info.params.exposure = payload.params.exposure ?? 0
          info.params.hmirror = !!payload.params.hmirror
          info.params.vflip = !!payload.params.vflip
          info.params.quality = payload.params.quality ?? 12
        }

        // 同步到可编辑参数（仅当未处于"已修改"状态）
        if (!paramsChanged.value) {
          syncInfoToParams()
        }
      }
    } catch (err) {
      console.error('[Video] 获取摄像头信息失败:', err)
    } finally {
      loading.value = false
    }
  }

  const refreshFramesizes = async () => {
    try {
      const res = await getVideoFramesizes()
      const payload = res.data?.data
      if (payload?.list && Array.isArray(payload.list)) {
        framesizeList.value = payload.list.map(item => ({
          id: item.id,
          name: item.name,
          width: item.width,
          height: item.height
        }))
      }
    } catch (err) {
      console.error('[Video] 获取分辨率列表失败:', err)
      // 提供默认回退
      framesizeList.value = [
        { id: 5, name: '320x240 (QVGA)', width: 320, height: 240 },
        { id: 8, name: '800x600 (SVGA)', width: 800, height: 600 },
        { id: 9, name: '1024x768 (XGA)', width: 1024, height: 768 },
        { id: 10, name: '1280x720 (HD)', width: 1280, height: 720 }
      ]
    }
  }

  // ========================================
  // 状态轮询
  // ========================================

  const startStatusPolling = () => {
    if (statusTimer) return
    statusTimer = setInterval(async () => {
      // 实时流中：不轮询（减少请求），只轮询简化状态
      if (isStreaming.value) {
        try {
          const res = await getVideoStatus()
          const payload = res.data?.data
          if (payload) {
            isStreaming.value = !!payload.streaming
          }
        } catch (_) { /* 静默失败 */ }
      } else {
        refreshInfo()
      }
    }, 5000)
  }

  const stopStatusPolling = () => {
    if (statusTimer) {
      clearInterval(statusTimer)
      statusTimer = null
    }
  }

  // ========================================
  // FPS 计数器
  // ========================================

  const startFpsCounter = () => {
    fpsFrameCount = 0
    fps.value = 0
    if (fpsTimer) clearInterval(fpsTimer)
    fpsTimer = setInterval(() => {
      fps.value = fpsFrameCount
      fpsFrameCount = 0
    }, 1000)
  }

  const stopFpsCounter = () => {
    if (fpsTimer) {
      clearInterval(fpsTimer)
      fpsTimer = null
    }
    fps.value = 0
  }

  // 每加载成功一帧就计数（快照模式下的占位）
  const onSnapshotLoad = () => {
    fpsFrameCount++
  }

  const onStreamLoad = () => {
    // MJPEG 流只触发一次 load，后续帧由浏览器自动渲染
  }

  const onSnapshotError = () => {
    console.warn('[Video] 快照加载失败')
  }

  const onStreamError = () => {
    ElMessage.error('视频流连接失败')
    handleStopStream()
  }

  // ========================================
  // 用户操作
  // ========================================

  const markParamsChanged = () => {
    paramsChanged.value = true
  }

  const handleStartStream = async () => {
    actionLoading.value = 'start'
    try {
      await startVideoStream()
      isStreaming.value = true
      startFpsCounter()
      // 前端实际使用 MJPEG img src，这里已经生效
      ElMessage.success('实时视频流已启动')
    } catch (err) {
      console.error('[Video] 启动流失败:', err)
      ElMessage.error('启动实时流失败')
    } finally {
      actionLoading.value = ''
    }
  }

  const handleStopStream = async (silent = false) => {
    actionLoading.value = 'stop'
    try {
      await stopVideoStream()
    } catch (_) { /* 忽略停止时的网络错误 */ }

    isStreaming.value = false
    stopFpsCounter()

    if (!silent) {
      ElMessage.info('实时视频流已停止')
    }
    // 刷新一次快照
    snapshotKey.value++
    actionLoading.value = ''
  }

  const handleTakeSnapshot = () => {
    // 更新时间戳 key，重新加载快照 img
    snapshotKey.value++
    fpsFrameCount++
    ElMessage.success('已刷新快照')
  }

  const handleDownloadSnapshot = async () => {
    try {
      // 使用 fetch 确保能拿到最新图片
      const url = getVideoSnapshotUrl()
      const response = await fetch(url)
      const blob = await response.blob()

      const link = document.createElement('a')
      const ts = new Date().toISOString().replace(/[:.]/g, '-')
      link.download = `snapshot_${ts}.jpg`
      link.href = URL.createObjectURL(blob)
      link.click()
      setTimeout(() => URL.revokeObjectURL(link.href), 1000)

      ElMessage.success('快照已下载')
    } catch (err) {
      console.error('[Video] 下载失败:', err)
      ElMessage.error('下载失败')
    }
  }

  const applyParams = async () => {
    actionLoading.value = 'config'
    try {
      const payload = {
        brightness: params.brightness,
        contrast: params.contrast,
        saturation: params.saturation,
        hmirror: params.hmirror,
        vflip: params.vflip,
        framesize: params.framesize,
        quality: params.quality
      }
      const res = await setVideoConfig(payload)
      if (res.data?.status === 'success') {
        ElMessage.success(res.data.message || '参数已应用')
        // 刷新后端信息
        await refreshInfo()
        // 刷新画面
        if (!isStreaming.value) {
          snapshotKey.value++
        }
      } else {
        ElMessage.error(res.data?.message || '参数应用失败')
      }
    } catch (err) {
      console.error('[Video] 应用参数失败:', err)
      ElMessage.error('参数应用失败')
    } finally {
      actionLoading.value = ''
    }
  }

  // ========================================
  // 初始化 / 清理
  // ========================================

  onMounted(async () => {
    await Promise.all([
      refreshInfo(),
      refreshFramesizes()
    ])
    startStatusPolling()
  })

  onUnmounted(() => {
    stopStatusPolling()
    stopFpsCounter()
    if (isStreaming.value) {
      handleStopStream(true) // 离开页面自动停止
    }
  })

  return {
    // 状态
    loading,
    actionLoading,
    isStreaming,
    fps,
    snapshotKey,
    paramsChanged,
    info,
    params,
    framesizeList,
    // URL
    streamUrl,
    snapshotUrl,
    // 数据获取
    refreshInfo,
    refreshFramesizes,
    // 图片事件
    onSnapshotLoad,
    onSnapshotError,
    onStreamLoad,
    onStreamError,
    // 用户操作
    markParamsChanged,
    handleStartStream,
    handleStopStream,
    handleTakeSnapshot,
    handleDownloadSnapshot,
    applyParams
  }
}
