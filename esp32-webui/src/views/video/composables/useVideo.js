import { ref, reactive, onMounted, onUnmounted, computed } from 'vue'
import { ElMessage } from 'element-plus'
import {
  getVideoInfo,
  getVideoFramesizes,
  getVideoSnapshotUrl,
  setVideoConfig,
  stopVideoStream
} from '@/api/esp32'

// WebSocket URL - 相对路径会自动通过 Vite 代理
const getWsUrl = () => {
  const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:'
  const host = window.__ESP32_HOST__ || window.location.host
  return `${protocol}//${host}/api/video/ws`
}

export function useVideo() {
  // ========================================
  // 状态
  // ========================================
  const loading = ref(false)
  const actionLoading = ref('')
  const isStreaming = ref(false)
  const fps = ref(0)
  const snapshotKey = ref(0)
  const paramsChanged = ref(false)

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

  const params = reactive({
    brightness: 0,
    contrast: 0,
    saturation: 0,
    gain: 0,
    exposure: 0,
    hmirror: false,
    vflip: false,
    framesize: 8,
    quality: 12,
    awb: true,
    wb_mode: 0,
    aec: true,
    agc: true
  })

  const framesizeList = ref([])

  let statusTimer = null
  let fpsTimer = null
  let fpsFrameCount = 0
  let ws = null
  let imgRef = null // 保存 img 元素引用
  let currentBlobUrl = null // 保存当前显示的 Blob 临时 URL

  // ========================================
  // URL 计算
  // ========================================
  const snapshotUrl = computed(() => {
    return `${getVideoSnapshotUrl()}`
  })

  // ========================================
  // WebSocket 连接管理 (核心修复)
  // ========================================
  const connectWs = () => {
    if (ws) {
      ws.close()
    }

    const wsUrl = getWsUrl()
    console.log('[Video] 连接 WebSocket:', wsUrl)

    // 使用动态获取的 wsUrl，不要写死 IP
    ws = new WebSocket(wsUrl)
    // 关键1：强制改为 arraybuffer 解析
    ws.binaryType = 'arraybuffer'

    ws.onopen = () => {
      console.log('[Video] WebSocket 已连接，发送激活指令')
      ws.send('start') // 必须发送初始化指令唤醒 ESP32
    }

    ws.onmessage = (event) => {
      // 关键2：处理二进制 ArrayBuffer 图像帧
      if (event.data instanceof ArrayBuffer) {
        if (!imgRef) return

        // 转化 ArrayBuffer 为 JPEG Blob
        const blob = new Blob([event.data], { type: 'image/jpeg' })
        const newUrl = URL.createObjectURL(blob)

        // 立即替换图片源
        imgRef.src = newUrl

        // 同步清理上一帧的内存，防止微任务积压导致浏览器崩溃
        if (currentBlobUrl) {
          URL.revokeObjectURL(currentBlobUrl)
        }
        currentBlobUrl = newUrl

        fpsFrameCount++
      } else if (typeof event.data === 'string') {
        console.log('[Video] WS 文本消息:', event.data)
      }
    }

    ws.onerror = (err) => {
      console.error('[Video] WebSocket 错误:', err)
      ElMessage.error('视频流连接失败')
    }

    ws.onclose = () => {
      console.log('[Video] WebSocket 已关闭')
      if (currentBlobUrl) {
        URL.revokeObjectURL(currentBlobUrl)
        currentBlobUrl = null
      }
      if (isStreaming.value) {
        isStreaming.value = false
        stopFpsCounter()
      }
    }
  }

  const disconnectWs = () => {
    if (ws) {
      ws.close()
      ws = null
    }
  }

  // 设置 img 元素引用
  const setImgRef = (el) => {
    imgRef = el
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

  // ========================================
  // 数据获取
  // ========================================
  const syncInfoToParams = () => {
    params.brightness = info.params.brightness
    params.contrast = info.params.contrast
    params.saturation = info.params.saturation
    params.hmirror = info.params.hmirror
    params.vflip = info.params.vflip
    params.framesize = info.resolution.framesize || 8
    params.quality = info.params.quality
    params.awb = info.params.awb ?? true
    params.wb_mode = info.params.wb_mode ?? 0
    params.aec = info.params.aec ?? true
    params.agc = info.params.agc ?? true
    paramsChanged.value = false
  }

  const refreshInfo = async () => {
    loading.value = true
    try {
      const res = await getVideoInfo()
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
        if (!paramsChanged.value) syncInfoToParams()
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
    statusTimer = setInterval(() => {
      if (isStreaming.value) return
      refreshInfo()
    }, 5000)
  }

  const stopStatusPolling = () => {
    if (statusTimer) {
      clearInterval(statusTimer)
      statusTimer = null
    }
  }

  // ========================================
  // 用户操作
  // ========================================
  const handleStartStream = () => {
    if (!info.initialized) {
      ElMessage.warning('摄像头未初始化')
      return
    }
    isStreaming.value = true
    startFpsCounter()
    connectWs()
    ElMessage.success('视频流已启动')
  }

  const handleStopStream = async (silent = false) => {
    actionLoading.value = 'stop'
    try {
      await stopVideoStream()
    } catch (_) { /* ignore */ }
    disconnectWs()
    isStreaming.value = false
    stopFpsCounter()
    if (!silent) ElMessage.info('视频流已停止')
    snapshotKey.value++
    actionLoading.value = ''
  }

  const handleTakeSnapshot = () => {
    snapshotKey.value++
    ElMessage.success('已刷新快照')
  }

  const handleDownloadSnapshot = async () => {
    try {
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
      ElMessage.error('下载失败')
    }
  }

  const markParamsChanged = () => {
    paramsChanged.value = true
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
        quality: params.quality,
        awb: params.awb,
        wb_mode: params.wb_mode,
        aec: params.aec,
        agc: params.agc
      }
      const res = await setVideoConfig(payload)
      if (res.data?.status === 'success') {
        ElMessage.success(res.data.message || '参数已应用')
        await refreshInfo()
        if (!isStreaming.value) snapshotKey.value++
      } else {
        ElMessage.error(res.data?.message || '参数应用失败')
      }
    } catch (err) {
      ElMessage.error('参数应用失败')
    } finally {
      actionLoading.value = ''
    }
  }

  // 图片事件
  const onSnapshotLoad = () => { fpsFrameCount++ }
  const onSnapshotError = () => console.warn('[Video] 快照加载失败')

  // ========================================
  // 生命周期
  // ========================================
  onMounted(async () => {
    await Promise.all([refreshInfo(), refreshFramesizes()])
    startStatusPolling()
  })

  onUnmounted(() => {
    stopStatusPolling()
    stopFpsCounter()
    disconnectWs()
  })

  return {
    loading,
    actionLoading,
    isStreaming,
    fps,
    snapshotKey,
    paramsChanged,
    info,
    params,
    framesizeList,
    snapshotUrl,
    refreshInfo,
    refreshFramesizes,
    onSnapshotLoad,
    onSnapshotError,
    markParamsChanged,
    handleStartStream,
    handleStopStream,
    handleTakeSnapshot,
    handleDownloadSnapshot,
    applyParams,
    setImgRef
  }
}