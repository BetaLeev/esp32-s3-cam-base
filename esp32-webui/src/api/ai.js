/**
 * AI 语音交互 API
 * 支持 WebSocket 音频流传输和语音识别
 */
import axios from 'axios'

const baseURL = import.meta.env.VITE_API_BASE_URL
const api = axios.create({
  baseURL: baseURL ? `${baseURL}/api` : '/api',
  timeout: 30000,
  headers: {
    'Content-Type': 'application/json'
  }
})

// ========================================
// 时间戳工具
// ========================================

/**
 * 获取当前时间戳（毫秒）
 */
const getTimestamp = () => Date.now()

/**
 * 格式化时间戳为可读字符串
 * @param {number} ts - 时间戳（毫秒）
 * @returns {string} 格式化的时间字符串
 */
const formatTimestamp = (ts) => {
  const date = new Date(ts)
  return date.toISOString().replace('T', ' ').replace('Z', '') +
    `.${String(ts % 1000).padStart(3, '0')}`
}

/**
 * 创建带时间戳的日志
 * @param {string} event - 事件名称
 * @param {object} data - 附加数据
 * @returns {object} 包含 timestamp, event, data 的对象
 */
const createTimestampLog = (event, data = {}) => {
  return {
    timestamp: getTimestamp(),
    formattedTime: formatTimestamp(getTimestamp()),
    event,
    data
  }
}

// ========================================
// WebSocket URL 构建
// ========================================
// ESP32 设备地址
const ESP32_HOST = '192.168.102.158'

const getWsUrl = () => {
  const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:'
  // 开发模式：直接连接到 ESP32
  // 生产模式：通过相对路径
  if (window.location.hostname === 'localhost' || window.location.hostname === '127.0.0.1') {
    return `${protocol}//${ESP32_HOST}/api/ai/ws`
  }
  return `${protocol}//${window.location.host}/api/ai/ws`
}

const getBaseUrl = () => {
  // 开发模式：直接连接到 ESP32
  if (window.location.hostname === 'localhost' || window.location.hostname === '127.0.0.1') {
    return `http://${ESP32_HOST}`
  }
  return ''
}

// ========================================
// WebSocket 音频服务
// ========================================

/**
 * AI 语音 WebSocket 服务类
 */
class AiVoiceService {
  constructor() {
    this.ws = null
    this.reconnectAttempts = 0
    this.maxReconnectAttempts = 3
    this.reconnectDelay = 1000
    this.onStatusChange = null      // 连接状态回调
    this.onRecognitionResult = null // 识别结果回调
    this.onError = null             // 错误回调
    this.audioChunks = []           // 接收的音频数据
    this.testMode = false           // 本地测试模式

    // TTS 配置
    this.ttsEnabled = true          // TTS 开关
    this.ttsRate = 1.0              // 语速 (0.1 - 10)
    this.ttsPitch = 1.0             // 音调 (0 - 2)
    this.ttsVolume = 1.0            // 音量 (0 - 1)
    this.ttsLang = 'zh-CN'          // 语言

    // 性能追踪
    this.performanceLog = []        // 性能日志数组
    this.enablePerformanceLog = true // 是否启用性能日志
  }

  /**
   * 启用/禁用测试模式
   */
  setTestMode(enabled) {
    this.testMode = enabled
    console.log('[AiVoice] 测试模式:', enabled ? '启用' : '禁用')
  }

  /**
   * 记录性能日志
   */
  logPerformance(event, data = {}) {
    if (!this.enablePerformanceLog) return

    const log = createTimestampLog(event, data)
    this.performanceLog.push(log)

    // 控制台输出
    console.log(
      `[AiVoice-Perf] [${log.formattedTime}] ${event}`,
      data.audioSize ? `| 音频大小: ${(data.audioSize / 1024).toFixed(2)} KB` : '',
      data.delay ? `| 延迟: ${data.delay}ms` : ''
    )

    // 保留最近100条日志
    if (this.performanceLog.length > 100) {
      this.performanceLog.shift()
    }

    return log
  }

  /**
   * 获取性能日志摘要
   */
  getPerformanceSummary() {
    const logs = this.performanceLog
    if (logs.length < 2) return null

    const summary = {
      totalEvents: logs.length,
      timeline: []
    }

    // 构建时间线
    for (const log of logs) {
      summary.timeline.push({
        time: log.formattedTime,
        event: log.event,
        data: log.data
      })
    }

    // 计算关键延迟
    const firstLog = logs[0]
    const lastLog = logs[logs.length - 1]
    summary.totalDuration = lastLog.timestamp - firstLog.timestamp

    return summary
  }

  /**
   * 清除性能日志
   */
  clearPerformanceLog() {
    this.performanceLog = []
    console.log('[AiVoice-Perf] 性能日志已清除')
  }

  /**
   * 打印完整的性能时间线
   */
  printPerformanceTimeline() {
    const summary = this.getPerformanceSummary()
    if (!summary) {
      console.log('[AiVoice-Perf] 日志不足，无法生成时间线')
      return
    }

    console.group('[AiVoice-Perf] ===== 性能时间线 =====')
    console.log(`总事件数: ${summary.totalEvents}`)
    console.log(`总耗时: ${summary.totalDuration}ms (${(summary.totalDuration / 1000).toFixed(2)}s)`)
    console.log('---')

    for (let i = 0; i < summary.timeline.length; i++) {
      const item = summary.timeline[i]
      const prevItem = i > 0 ? summary.timeline[i - 1] : null
      const gap = prevItem ? item.timestamp - prevItem.timestamp : 0

      console.log(
        `[${item.time}] ${item.event}`,
        gap > 0 ? `(+${gap}ms)` : '',
        item.data.audioSize ? `(${Math.round(item.data.audioSize / 1024)}KB)` : '',
        item.data.command ? `(${item.data.command})` : ''
      )
    }

    console.groupEnd()
  }

  /**
   * 连接 WebSocket（真实或模拟）
   */
  connect() {
    // 测试模式：模拟连接
    if (this.testMode) {
      console.log('[AiVoice] 测试模式：模拟连接')
      setTimeout(() => {
        this.updateStatus('connected')
      }, 500)
      return
    }

    if (this.ws && this.ws.readyState === WebSocket.OPEN) {
      console.log('[AiVoice] WebSocket 已连接')
      return
    }

    const wsUrl = getWsUrl()
    console.log('[AiVoice] 连接 WebSocket:', wsUrl)

    this.ws = new WebSocket(wsUrl)
    this.ws.binaryType = 'arraybuffer'

    this.ws.onopen = () => {
      console.log('[AiVoice] WebSocket 已连接')
      this.reconnectAttempts = 0
      this.updateStatus('connected')
    }

    this.ws.onmessage = (event) => {
      this.handleMessage(event)
    }

    this.ws.onerror = (err) => {
      console.error('[AiVoice] WebSocket 错误:', err)
      this.updateStatus('error')
      if (this.onError) {
        this.onError('连接失败')
      }
    }

    this.ws.onclose = () => {
      console.log('[AiVoice] WebSocket 已关闭')
      this.updateStatus('disconnected')
      this.attemptReconnect()
    }
  }

  /**
   * 处理接收到的消息
   */
  handleMessage(event) {
    if (event.data instanceof ArrayBuffer) {
      // 接收到的音频数据（未来用于 TTS 播放）
      this.audioChunks.push(event.data)
      return
    }

    // 解析 JSON 消息
    try {
      const data = JSON.parse(event.data)
      console.log('[AiVoice] 收到消息:', data)

      switch (data.type) {
        case 'recognition_result':
          // 记录收到识别结果的性能日志
          this.logPerformance('收到识别结果', {
            command: data.command,
            text: data.text,
            action: data.action
          })

          // 打印完整时间线（如果是识别结果）
          this.printPerformanceTimeline()

          if (this.onRecognitionResult) {
            this.onRecognitionResult(data)
          }
          break

        case 'status':
          this.updateStatus(data.status)
          break

        case 'error':
          this.logPerformance('收到错误', { message: data.message })
          if (this.onError) {
            this.onError(data.message)
          }
          break

        case 'ack':
          console.log('[AiVoice] 服务已确认音频数据')
          break

        default:
          console.log('[AiVoice] 未知消息类型:', data.type)
      }
    } catch (err) {
      console.error('[AiVoice] 消息解析失败:', err)
    }
  }

  /**
   * 发送音频数据
   * @param {Blob|ArrayBuffer} audioData - 音频数据
   */
  sendAudio(audioData) {
    // 获取音频大小
    const audioSize = audioData.size || audioData.byteLength || 0

    // 测试模式：模拟发送并返回识别结果
    if (this.testMode) {
      // 记录发送音频
      this.logPerformance('发送音频(测试)', {
        audioSize,
        audioSizeKB: (audioSize / 1024).toFixed(2)
      })

      // 模拟识别延迟
      setTimeout(() => {
        const commands = [
          { text: '打开灯', command: 'led_on', action: '已打开LED灯' },
          { text: '关闭灯', command: 'led_off', action: '已关闭LED灯' },
          { text: '打开水泵', command: 'pump_on', action: '已启动水泵' },
          { text: '关闭水泵', command: 'pump_off', action: '已停止水泵' },
          { text: '查询温度', command: 'query_temp', action: '当前温度为 25.5°C' },
          { text: '查询湿度', command: 'query_humidity', action: '当前湿度为 60%' },
        ]
        const randomCmd = commands[Math.floor(Math.random() * commands.length)]

        this.handleTestRecognitionResult(randomCmd)
      }, 1500 + Math.random() * 1000)

      return true
    }

    // 真实模式
    if (!this.ws || this.ws.readyState !== WebSocket.OPEN) {
      console.warn('[AiVoice] WebSocket 未连接')
      return false
    }

    // 记录发送音频
    this.logPerformance('发送音频', { audioSize })

    this.ws.send(audioData)
    return true
  }

  /**
   * 测试模式：处理模拟识别结果
   */
  handleTestRecognitionResult(result) {
    // 记录收到识别结果
    this.logPerformance('收到识别结果(测试)', {
      command: result.command,
      text: result.text,
      action: result.action
    })

    // 打印完整时间线
    this.printPerformanceTimeline()

    if (this.onRecognitionResult) {
      this.onRecognitionResult(result)
    }
  }

  /**
   * 发送文本消息（备用方案）
   * @param {string} text - 文本消息
   */
  sendText(text) {
    if (!this.ws || this.ws.readyState !== WebSocket.OPEN) {
      console.warn('[AiVoice] WebSocket 未连接')
      return false
    }

    // 直接发送文本内容
    this.ws.send(text)
    return true
  }

  /**
   * 发送语音开始信号
   */
  sendStart() {
    if (this.testMode) {
      console.log('[AiVoice] 测试模式：发送开始信号')
      return true
    }
    if (!this.ws || this.ws.readyState !== WebSocket.OPEN) {
      return false
    }
    this.ws.send(JSON.stringify({ type: 'start' }))
    return true
  }

  /**
   * 发送语音结束信号
   */
  sendEnd() {
    if (this.testMode) {
      console.log('[AiVoice] 测试模式：发送结束信号')
      return true
    }
    if (!this.ws || this.ws.readyState !== WebSocket.OPEN) {
      return false
    }
    this.ws.send(JSON.stringify({ type: 'end' }))
    return true
  }

  /**
   * 断开连接
   */
  disconnect() {
    this.reconnectAttempts = this.maxReconnectAttempts // 禁止重连
    if (this.testMode) {
      this.updateStatus('disconnected')
      return
    }
    if (this.ws) {
      this.ws.close()
      this.ws = null
    }
  }

  /**
   * 重连
   */
  reconnect() {
    this.disconnect()
    this.reconnectAttempts = 0
    this.connect()
  }

  /**
   * 尝试重连
   */
  attemptReconnect() {
    if (this.reconnectAttempts >= this.maxReconnectAttempts) {
      console.log('[AiVoice] 达到最大重连次数')
      return
    }

    this.reconnectAttempts++
    console.log(`[AiVoice] ${this.reconnectDelay}ms 后尝试重连...`)

    setTimeout(() => {
      this.connect()
    }, this.reconnectDelay)
  }

  /**
   * 更新连接状态
   */
  updateStatus(status) {
    if (this.onStatusChange) {
      this.onStatusChange(status)
    }
  }

  /* ========================================
   * TTS 语音合成
   * ======================================== */

  /**
   * 检查浏览器是否支持 Web Speech API
   */
  isTTSSupported() {
    return 'speechSynthesis' in window
  }

  /**
   * 使用浏览器 TTS 播放文本
   * @param {string} text - 要播放的文本
   * @param {object} options - 可选配置
   */
  speak(text, options = {}) {
    if (!this.isTTSSupported()) {
      console.warn('[AiVoice] 浏览器不支持 TTS')
      return
    }

    if (!this.ttsEnabled && !options.force) {
      console.log('[AiVoice] TTS 已禁用')
      return
    }

    // 停止当前播放
    this.stopSpeaking()

    const utterance = new SpeechSynthesisUtterance(text)

    // 配置
    utterance.lang = options.lang || this.ttsLang
    utterance.rate = options.rate || this.ttsRate
    utterance.pitch = options.pitch || this.ttsPitch
    utterance.volume = options.volume !== undefined ? options.volume : this.ttsVolume

    // 自动选择最佳中文语音
    const voice = this.selectBestVoice(utterance.lang)
    if (voice) {
      utterance.voice = voice
      console.log(`[AiVoice TTS] 使用语音: ${voice.name}`)
    }

    // 调试日志
    console.log(`[AiVoice TTS] 播放: "${text}"`, {
      lang: utterance.lang,
      rate: utterance.rate,
      pitch: utterance.pitch,
      voice: voice ? voice.name : 'default'
    })

    // 播放
    window.speechSynthesis.speak(utterance)
  }

  /**
   * 选择最佳语音
   * @param {string} lang - 语言代码
   */
  selectBestVoice(lang) {
    const voices = window.speechSynthesis.getVoices()

    // 过滤指定语言的语音
    const langVoices = voices.filter(v => v.lang.includes(lang.split('-')[0]))

    if (langVoices.length === 0) {
      return null
    }

    // 优先级：
    // 1. 本地语音（更稳定）
    // 2. Google 中文语音
    // 3. Microsoft 中文语音
    // 4. 其他中文语音
    const priorityPatterns = ['Google', 'Microsoft', 'Apple']

    for (const pattern of priorityPatterns) {
      const voice = langVoices.find(v => v.name.includes(pattern))
      if (voice) return voice
    }

    // 返回第一个本地语音
    return langVoices.find(v => v.localService) || langVoices[0]
  }

  /**
   * 停止当前播放
   */
  stopSpeaking() {
    if (this.isTTSSupported()) {
      window.speechSynthesis.cancel()
    }
  }

  /**
   * 获取可用的语音列表
   */
  getVoices() {
    if (!this.isTTSSupported()) {
      return []
    }
    // 确保语音列表已加载
    return window.speechSynthesis.getVoices()
  }

  /**
   * 使用浏览器原生 TTS（跨平台兼容）
   * @param {string} text - 要播放的文本
   * @param {string} voice - 语音名称
   */
  speakWithEdgeTTS(text, voice = '中文女') {
    // 清理文本
    const cleanText = text.replace(/[^\u4e00-\u9fa5a-zA-Z0-9\s。，！？、：；""''（）《》【】]/g, '')

    if (!cleanText) {
      console.warn('[AiVoice] 文本为空')
      return
    }

    console.log(`[AiVoice TTS] 开始播放: "${text}" (语音: ${voice})`)

    // 停止当前播放
    this.stopSpeaking()

    const utterance = new SpeechSynthesisUtterance(cleanText)

    // 选择中文语音
    utterance.lang = 'zh-CN'
    utterance.rate = 0.9   // 稍慢，更自然
    utterance.pitch = 1.1  // 稍高，更有活力
    utterance.volume = this.ttsVolume

    // 尝试选择最好的中文语音
    const voices = window.speechSynthesis.getVoices()
    if (voices.length > 0) {
      // 优先选择中文语音
      const chineseVoice = voices.find(v =>
        v.lang.includes('zh') && v.localService
      ) || voices.find(v => v.lang.includes('zh')) || voices[0]

      if (chineseVoice) {
        utterance.voice = chineseVoice
        console.log(`[AiVoice TTS] 使用语音: ${chineseVoice.name}`)
      }
    }

    // 播放
    window.speechSynthesis.speak(utterance)
  }

  /**
   * 统一播放接口（自动选择最佳方案）
   * @param {string} text - 要播放的文本
   * @param {object} options - 配置选项
   */
  playText(text, options = {}) {
    // 优先使用 Google TTS（如果可用）
    if (options.useGoogle !== false) {
      this.speakWithGoogle(text)
    } else {
      this.speak(text, options)
    }
  }

  /**
   * 获取中文语音
   */
  getChineseVoice() {
    const voices = this.getVoices()
    // 优先选择中文语音
    return voices.find(v => v.lang.includes('zh')) || voices[0]
  }

  /**
   * 启用/禁用 TTS
   */
  setTTSEnabled(enabled) {
    this.ttsEnabled = enabled
    console.log('[AiVoice] TTS:', enabled ? '启用' : '禁用')
    if (!enabled) {
      this.stopSpeaking()
    }
  }

  /**
   * 设置 TTS 配置
   */
  setTTSConfig(config) {
    if (config.rate !== undefined) this.ttsRate = config.rate
    if (config.pitch !== undefined) this.ttsPitch = config.pitch
    if (config.volume !== undefined) this.ttsVolume = config.volume
    if (config.lang !== undefined) this.ttsLang = config.lang
    console.log('[AiVoice] TTS 配置已更新', {
      rate: this.ttsRate,
      pitch: this.ttsPitch,
      volume: this.ttsVolume,
      lang: this.ttsLang
    })
  }

  /**
   * 处理识别结果并播放 TTS
   * @param {object} data - 识别结果数据
   */
  speakResult(data) {
    if (!data) return

    const textToSpeak = data.action || data.reply || data.text
    if (textToSpeak) {
      this.speak(textToSpeak)
    }
  }
}

// 导出单例
export const aiVoiceService = new AiVoiceService()

// ========================================
// 音频录制工具
// ========================================

/**
 * 浏览器音频录制器
 */
class AudioRecorder {
  constructor() {
    this.mediaRecorder = null
    this.audioChunks = []
    this.stream = null
    this.isRecording = false
    this.onDataAvailable = null
    this.mimeType = 'audio/webm;codecs=opus'
  }

  /**
   * 请求麦克风权限并初始化
   */
  async init() {
    try {
      this.stream = await navigator.mediaDevices.getUserMedia({
        audio: {
          echoCancellation: true,      // 回声消除
          noiseSuppression: true,      // 降噪
          autoGainControl: true,       // 自动增益
          channelCount: 1,            // 单声道
          sampleRate: 16000            // 采样率 16kHz（ESP32 语音识别推荐）
        }
      })
      console.log('[AudioRecorder] 麦克风初始化成功')
      return true
    } catch (err) {
      console.error('[AudioRecorder] 麦克风初始化失败:', err)
      return false
    }
  }

  /**
   * 开始录制
   */
  start() {
    if (this.isRecording || !this.stream) {
      return false
    }

    this.audioChunks = []

    try {
      this.mediaRecorder = new MediaRecorder(this.stream, {
        mimeType: this.mimeType
      })
    } catch (err) {
      // 降级到默认格式
      console.warn('[AudioRecorder] 不支持 webm，尝试默认格式')
      this.mediaRecorder = new MediaRecorder(this.stream)
    }

    this.mediaRecorder.ondataavailable = (event) => {
      if (event.data && event.data.size > 0) {
        this.audioChunks.push(event.data)
        if (this.onDataAvailable) {
          this.onDataAvailable(event.data)
        }
      }
    }

    this.mediaRecorder.start(100) // 每100ms发送一个数据块
    this.isRecording = true
    console.log('[AudioRecorder] 开始录制')
    return true
  }

  /**
   * 停止录制
   * @returns {Promise<Blob>} 录制的音频 Blob
   */
  async stop() {
    return new Promise((resolve, reject) => {
      if (!this.isRecording || !this.mediaRecorder) {
        resolve(null)
        return
      }

      this.mediaRecorder.onstop = () => {
        const blob = new Blob(this.audioChunks, { type: this.mimeType })
        this.audioChunks = []
        this.isRecording = false
        console.log('[AudioRecorder] 停止录制，音频大小:', blob.size)
        resolve(blob)
      }

      this.mediaRecorder.onerror = (err) => {
        reject(err)
      }

      this.mediaRecorder.stop()
    })
  }

  /**
   * 释放资源
   */
  release() {
    if (this.stream) {
      this.stream.getTracks().forEach(track => track.stop())
      this.stream = null
    }
    this.mediaRecorder = null
    this.isRecording = false
    console.log('[AudioRecorder] 资源已释放')
  }

  /**
   * 获取音频权限状态
   */
  async getPermissionStatus() {
    try {
      const result = await navigator.permissions.query({ name: 'microphone' })
      return result.state
    } catch (err) {
      return 'prompt'
    }
  }
}

// 导出录制器类
export { AudioRecorder }

// ========================================
// HTTP API 接口
// ========================================

/**
 * 获取AI服务状态
 * @returns {Promise<Object>} 状态信息
 */
export const getAiStatus = async () => {
  try {
    const res = await api.get('/ai/status')
    return res.data
  } catch (err) {
    console.error('[Ai API] 获取状态失败:', err)
    throw err
  }
}

/**
 * 获取支持的命令列表
 * @returns {Promise<Array>} 命令列表
 */
export const getCommandList = async () => {
  try {
    const res = await api.get('/ai/commands')
    return res.data?.data?.list || []
  } catch (err) {
    console.error('[Ai API] 获取命令列表失败:', err)
    return []
  }
}

/**
 * 发送文本聊天消息
 * @param {string} message - 消息内容
 * @returns {Promise<string>} AI回复
 */
export const sendChatMessage = async (message) => {
  try {
    const res = await api.post('/ai/chat', { message })
    return res.data?.data?.reply || ''
  } catch (err) {
    console.error('[Ai API] 发送消息失败:', err)
    throw err
  }
}

// 兼容旧代码的 Mock 实现
let mockConnected = true

/**
 * 获取AI对话历史
 */
export const getChatHistory = async () => {
  return []
}

/**
 * 清空对话历史
 */
export const clearChatHistory = async () => {
  return
}

/**
 * 获取AI服务状态（兼容旧代码）
 */
export const getAiServiceStatus = async () => {
  try {
    const res = await getAiStatus()
    return res.data?.online || false
  } catch {
    return mockConnected
  }
}

// 更新 Mock 连接状态（供调试用）
export const setMockConnected = (connected) => {
  mockConnected = connected
}

export default api
