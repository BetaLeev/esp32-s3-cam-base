import axios from 'axios'

const baseURL = import.meta.env.VITE_API_BASE_URL

const api = axios.create({
  baseURL: baseURL ? `${baseURL}/api` : '/api',
  timeout: 10000,
  headers: {
    'Content-Type': 'application/json'
  }
})

// ========================================
// 统一容错处理
// ========================================

/**
 * 带容错的API调用（axios）
 * 后端不可用时不抛出错误，返回 null
 * @param {Function} apiCall - API调用函数
 * @param {any} defaultValue - 默认值
 * @returns {Promise<{data: any, error: string|null}>}
 */
export async function safeApiCall(apiCall, defaultValue = null) {
  try {
    const response = await apiCall()
    return { data: response.data, error: null }
  } catch (err) {
    // 只记录错误，不抛出
    console.warn('[API] 请求失败:', err.message || err.code)
    return { data: defaultValue, error: err.message || '请求失败' }
  }
}

/**
 * 带容错的fetch调用
 * 后端不可用时不抛出错误，返回 null
 * @param {string} url - 请求URL
 * @param {any} defaultValue - 默认值
 * @returns {Promise<{data: any, error: string|null}>}
 */
export async function safeFetch(url, defaultValue = null) {
  try {
    const response = await fetch(url)
    if (!response.ok) {
      throw new Error(`HTTP ${response.status}`)
    }
    const data = await response.json()
    return { data, error: null }
  } catch (err) {
    console.warn('[API] Fetch 失败:', err.message)
    return { data: defaultValue, error: err.message }
  }
}

// ========================================
// 系统状态
// ========================================

// 系统完整状态
export const getStatus = () => api.get('/status')

// 带容错的系统状态获取
export async function getStatusSafe() {
  return safeApiCall(() => api.get('/status'), {})
}

// ========================================
// 网络状态
// ========================================

export const getNetwork = () => api.get('/network')

// 带容错的网络状态获取
export async function getNetworkSafe() {
  return safeApiCall(() => api.get('/network'), {
    connected: false,
    ssid: '',
    rssi: 0,
    ip: ''
  })
}

// ========================================
// 传感器
// ========================================

// 传感器数据
export const getSensorsData = () => api.get('/sensors/data')

// 带容错的传感器数据获取
export async function getSensorsDataSafe() {
  return safeApiCall(() => api.get('/sensors/data'), {
    thermistor: null,
    photosensor: null,
    dht11: null,
    soilhumidity: null
  })
}

// 传感器配置
export const getSensorsConfig = () => api.get('/sensors/config')

// 设置传感器配置
export const setSensorsConfig = (config) => api.post('/sensors/config', config)

// ========================================
// 执行器控制
// ========================================

// 水泵控制 - 档位: 0=关闭, 1=低速, 2=中速, 3=高速
export const controlPump = (params) => {
  return api.get('/pump', { params })
}

// 舵机控制 - 角度: 0-180
export const controlServo = (params) => {
  return api.get('/servo', { params })
}

// LED控制
export const controlLed = (params) => {
  return api.get('/led', { params })
}

// 获取已使用的引脚列表
export const getUsedPins = () => {
  return api.get('/gpio/used')
}

// 脉冲控制
export const controlPulse = (params) => {
  return api.get('/pulse', { params })
}

// ========================================
// 配置信息
// ========================================

export const getConfig = () => api.get('/config')

// ========================================
// 系统管理 (/api/system/*)
// ========================================

// 获取板子基本信息
export const getBoardInfo = () => api.get('/system/info')

// 带容错的板子信息获取
export async function getBoardInfoSafe() {
  return safeApiCall(() => api.get('/system/info'), {
    chip_model: '',
    firmware_version: '',
    board_name: '',
    build_time: '',
    uptime: '',
    uptime_seconds: 0,
    free_heap: ''
  })
}

// 获取温度数据
export const getBoardTemp = () => api.get('/system/temp')

// 带容错的温度数据获取
export async function getBoardTempSafe() {
  return safeApiCall(() => api.get('/system/temp'), {
    chip_temp: 0,
    ambient_temp: 0,
    cpu_temp: 0,
    sensor_ok: false
  })
}

// 系统重启
export const rebootSystem = () => api.post('/system/reboot')

// 系统关机 (深度睡眠)
export const shutdownSystem = (params) => api.post('/system/shutdown', params)

// ========================================
// TF卡文件管理
// ========================================

export const getStorageInfo = () => {
  const base = baseURL ? `${baseURL}` : ''
  return axios.get(`${base}/api/sdcard/info`, { timeout: 10000 })
}

// 带容错的存储信息获取
export async function getStorageInfoSafe() {
  return safeApiCall(() => {
    const base = baseURL ? `${baseURL}` : ''
    return axios.get(`${base}/api/sdcard/info`, { timeout: 10000 })
  }, { mounted: false, total: 0, used: 0 })
}

export const getFileList = (path) => {
  const base = baseURL ? `${baseURL}` : ''
  return axios.get(`${base}/api/sdcard/files?path=${path}`, { timeout: 10000 })
}

export const deleteFile = (path) => {
  const base = baseURL ? `${baseURL}` : ''
  return axios.post(`${base}/api/sdcard/delete`, { path }, { timeout: 10000 })
}

export const createDir = (path) => {
  const base = baseURL ? `${baseURL}` : ''
  return axios.post(`${base}/api/sdcard/mkdir`, { path }, { timeout: 10000 })
}

// 文件上传
export const uploadFile = (file, path = '') => {
  const base = baseURL ? `${baseURL}` : ''
  let url = `${base}/api/sdcard/upload?filename=${encodeURIComponent(file.name)}`
  if (path) {
    url += `&path=${encodeURIComponent(path)}`
  }

  return axios.post(url, file, {
    timeout: 60000,
    headers: {
      'Content-Type': 'application/octet-stream'
    }
  })
}

// 文件下载
export const getFileUrl = (path) => {
  return `/fs/files?path=${encodeURIComponent(path)}`
}

// ========================================
// 视频监控
// ========================================

export const getVideoInfo = () => api.get('/video/info')
export const getVideoStatus = () => api.get('/video/status')
export const getVideoFramesizes = () => api.get('/video/framesizes')

export const getVideoStreamUrl = () => {
  const base = baseURL ? `${baseURL}` : ''
  return `${base}/api/video/stream`
}

export const getVideoSnapshotUrl = () => {
  const base = baseURL ? `${baseURL}` : ''
  return `${base}/api/video/snapshot?t=${Date.now()}`
}

export const setVideoConfig = (config) => api.post('/video/config', config)
export const startVideoStream = () => api.post('/video/stream/start')
export const stopVideoStream = () => api.post('/video/stream/stop')

// ========================================
// 音频控制 (/api/audio/*)
// ========================================

// 获取音频状态
export const getAudioStatus = () => api.get('/audio/status')

// 带容错的音频状态获取
export async function getAudioStatusSafe() {
  return safeApiCall(() => api.get('/audio/status'), {
    initialized: false,
    state: 'uninit',
    gain: 2,
    gain_db: 9
  })
}

// 播放测试音调
export const playAudioTest = () => api.post('/audio/test')

// 停止播放
export const stopAudio = () => api.post('/audio/stop')

// 设置增益
export const setAudioGain = (gain) => api.post('/audio/gain', { gain })

// 设置软件音量
export const setAudioVolume = (volume) => api.post('/audio/volume', { volume })

// 播放TF卡音频文件
export const playAudioFile = (filePath) => {
  // URL 编码文件路径（处理中文和特殊字符）
  const encodedPath = encodeURIComponent(filePath)
  return api.post('/audio/play', { file: encodedPath })
}

export default api
