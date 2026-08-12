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
// Mock 数据 (开发环境使用)
// 后端接口就绪后，注释掉下面这行
// ========================================
// import '@/mock/esp32'

// ========================================
// 系统状态
// ========================================

// 系统完整状态
export const getStatus = () => api.get('/status')

// ========================================
// 网络状态
// ========================================

export const getNetwork = () => api.get('/network')

// ========================================
// 传感器
// ========================================

// 传感器数据
export const getSensorsData = () => api.get('/sensors/data')

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
export const controlServo = (angle) => {
  return api.get('/servo', { params: { angle } })
}

// LED控制
// params: {
//   pin: 2,              // GPIO引脚
//   action: 'start',     // start | stop | config
//   trigger_mode: 'blink', // static | blink
//   initial_level: 1,    // 初始电平 0=低 1=高
//   high_duration: 1,    // 高电平时长(秒)
//   low_duration: 1,     // 低电平时长(秒)
//   repeat_count: 3      // 重复次数，-1表示无限
// }
export const controlLed = (params) => {
  return api.get('/led', { params })
}

// 获取已使用的引脚列表 (需后端实现)
export const getUsedPins = () => {
  return api.get('/gpio/used')
}

// 脉冲控制
// params: {
//   pin: 2,              // GPIO引脚
//   action: 'start',     // start | stop
//   mode: 'single',      // single | continuous
//   intensity: 50,       // 强度 0-100%
//   frequency: 10,       // 频率 1-1000 Hz
//   pulse_width: 100     // 脉冲宽度 ms (单次模式)
// }
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

// 获取温度数据
export const getBoardTemp = () => api.get('/system/temp')

// 系统重启
export const rebootSystem = () => api.post('/system/reboot')

// 系统关机 (深度睡眠)
// params: { wakeup_pin: -1, wakeup_level: 0 }
export const shutdownSystem = (params) => api.post('/system/shutdown', params)

// ========================================
// TF卡文件管理
// ========================================

export const getStorageInfo = () => {
  const base = baseURL ? `${baseURL}` : ''
  return axios.get(`${base}/api/sdcard/info`, { timeout: 10000 })
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

// 文件上传 - 发送raw binary（后端直接读取body，不用FormData）
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

// 文件下载 - 使用相对路径，通过Vite代理
export const getFileUrl = (path) => {
  return `/fs/files?path=${encodeURIComponent(path)}`
}

// ========================================
// 视频监控
// ========================================

// 摄像头详细信息
export const getVideoInfo = () => api.get('/video/info')

// 摄像头状态（简化版，用于轮询）
export const getVideoStatus = () => api.get('/video/status')

// 支持的分辨率列表
export const getVideoFramesizes = () => api.get('/video/framesizes')

// 视频流 URL（直接用于 <img src>）
export const getVideoStreamUrl = () => {
  const base = baseURL ? `${baseURL}` : ''
  return `${base}/api/video/stream`
}

// 快照 URL（带时间戳防缓存）
export const getVideoSnapshotUrl = () => {
  const base = baseURL ? `${baseURL}` : ''
  return `${base}/api/video/snapshot?t=${Date.now()}`
}

// 设置摄像头参数
export const setVideoConfig = (config) => api.post('/video/config', config)

// 启动视频流
export const startVideoStream = () => api.post('/video/stream/start')

// 停止视频流
export const stopVideoStream = () => api.post('/video/stream/stop')

export default api
