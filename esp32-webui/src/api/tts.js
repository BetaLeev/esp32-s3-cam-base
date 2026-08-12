/**
 * @file tts.js
 * @brief TTS (Text-to-Speech) 模块
 *
 * 支持多种 TTS 服务：
 * - 浏览器原生 TTS
 * - 讯飞 TTS
 * - 百度 TTS
 * - 阿里云 TTS
 * - Edge TTS (需代理)
 * - CosyVoice (需部署)
 */

import { ref } from 'vue'

// ========================================
// TTS 提供商配置
// ========================================

export const TTS_PROVIDERS = {
  BROWSER: 'browser',      // 浏览器原生（免费，跨平台）
  XUNFEI: 'xunfei',        // 讯飞 TTS
  BAIDU: 'baidu',          // 百度 TTS
  ALIYUN: 'aliyun',         // 阿里云 TTS
  EDGE: 'edge',             // Edge TTS（需部署代理）
  COSYVOICE: 'cosyvoice',   // CosyVoice（需部署）
}

// TTS 配置
const ttsConfig = ref({
  provider: TTS_PROVIDERS.BROWSER,  // 当前提供商
  apiKey: '',                      // API Key
  apiSecret: '',                    // API Secret (部分服务需要)
  appId: '',                       // App ID (讯飞等需要)
  voice: 'zh-CN',                 // 默认语音
  rate: 1.0,                      // 语速
  pitch: 1.0,                      // 音调
  volume: 1.0                       // 音量
})

// 当前 TTS 实例
let currentTTS = null

// ========================================
// 抽象 TTS 类
// ========================================

class BaseTTS {
  constructor(config) {
    this.config = config
  }

  /**
   * 播放文本语音
   * @param {string} text - 要播放的文本
   * @returns {Promise<void>}
   */
  async speak(text) {
    throw new Error('子类必须实现 speak 方法')
  }

  /**
   * 停止播放
   */
  stop() {
    if (this.config.provider === TTS_PROVIDERS.BROWSER && 'speechSynthesis' in window) {
      window.speechSynthesis.cancel()
    }
  }
}

// ========================================
// 浏览器原生 TTS
// ========================================

class BrowserTTS extends BaseTTS {
  async speak(text) {
    this.stop()

    const utterance = new SpeechSynthesisUtterance(text)
    utterance.lang = this.config.voice || 'zh-CN'
    utterance.rate = this.config.rate
    utterance.pitch = this.config.pitch
    utterance.volume = this.config.volume

    // 选择语音
    const voices = window.speechSynthesis.getVoices()
    const voice = voices.find(v => v.lang.includes(this.config.voice.split('-')[0]))
    if (voice) {
      utterance.voice = voice
    }

    return new Promise((resolve, reject) => {
      utterance.onend = () => resolve()
      utterance.onerror = (e) => reject(e)
      window.speechSynthesis.speak(utterance)
    })
  }

  stop() {
    window.speechSynthesis.cancel()
  }
}

// ========================================
// 讯飞 TTS
// ========================================

class XunfeiTTS extends BaseTTS {
  // 讯飞 WebAPI TTS 需要自行实现鉴权和调用
  async speak(text) {
    // TODO: 实现讯飞 TTS 调用
    // 讯飞提供 WebAPI 和 实时语音合成 SDK
    // 参考: https://www.xfyun.cn/doc/tts/online_tts/API.html

    console.warn('[TTS] 讯飞 TTS 待接入，请配置 API Key')
    throw new Error('讯飞 TTS 未配置')
  }
}

// ========================================
// 百度 TTS
// ========================================

class BaiduTTS extends BaseTTS {
  async speak(text) {
    // TODO: 实现百度 TTS 调用
    // 百度提供免费额度
    // 参考: https://ai.baidu.com/ai-doc/Speech/ykda38diu

    console.warn('[TTS] 百度 TTS 待接入，请配置 API Key')
    throw new Error('百度 TTS 未配置')
  }
}

// ========================================
// 阿里云 TTS
// ========================================

class AliyunTTS extends BaseTTS {
  async speak(text) {
    // TODO: 实现阿里云 TTS 调用
    // 参考: https://help.aliyun.com/document_detail/27

    console.warn('[TTS] 阿里云 TTS 待接入，请配置 API Key')
    throw new Error('阿里云 TTS 未配置')
  }
}

// ========================================
// Edge TTS
// ========================================

class EdgeTTS extends BaseTTS {
  constructor(config) {
    super(config)
    this.apiUrl = config.apiUrl || ''  // 需部署代理服务
  }

  async speak(text) {
    if (!this.apiUrl) {
      throw new Error('Edge TTS 代理地址未配置')
    }

    // TODO: 调用 Edge TTS 代理 API
    // 需要部署 edge-tts-openai-cf-worker 或类似代理

    console.warn('[TTS] Edge TTS 待配置代理地址')
    throw new Error('Edge TTS 代理未配置')
  }
}

// ========================================
// CosyVoice
// ========================================

class CosyVoiceTTS extends BaseTTS {
  constructor(config) {
    super(config)
    this.apiUrl = config.apiUrl || ''  // CosyVoice 服务地址
  }

  async speak(text) {
    if (!this.apiUrl) {
      throw new Error('CosyVoice 服务地址未配置')
    }

    // TODO: 调用 CosyVoice API
    // 参考: https://github.com/FunAudioLLM/CosyVoice

    console.warn('[TTS] CosyVoice 待配置服务地址')
    throw new Error('CosyVoice 服务未配置')
  }
}

// ========================================
// TTS 管理器
// ========================================

class TTSManager {
  constructor() {
    this.currentTTS = null
    this.config = ttsConfig
  }

  /**
   * 初始化 TTS
   * @param {string} provider - TTS 提供商
   * @param {object} options - 配置选项
   */
  init(provider, options = {}) {
    this.config.value = { ...this.config.value, ...options }

    switch (provider) {
      case TTS_PROVIDERS.BROWSER:
        this.currentTTS = new BrowserTTS(this.config.value)
        break
      case TTS_PROVIDERS.XUNFEI:
        this.currentTTS = new XunfeiTTS(this.config.value)
        break
      case TTS_PROVIDERS.BAIDU:
        this.currentTTS = new BaiduTTS(this.config.value)
        break
      case TTS_PROVIDERS.ALIYUN:
        this.currentTTS = new AliyunTTS(this.config.value)
        break
      case TTS_PROVIDERS.EDGE:
        this.currentTTS = new EdgeTTS(this.config.value)
        break
      case TTS_PROVIDERS.COSYVOICE:
        this.currentTTS = new CosyVoiceTTS(this.config.value)
        break
      default:
        this.currentTTS = new BrowserTTS(this.config.value)
    }

    console.log(`[TTS] 已切换到: ${provider}`)
  }

  /**
   * 播放文本
   */
  async speak(text) {
    if (!this.currentTTS) {
      this.init(TTS_PROVIDERS.BROWSER)
    }
    return this.currentTTS.speak(text)
  }

  /**
   * 停止播放
   */
  stop() {
    if (this.currentTTS) {
      this.currentTTS.stop()
    }
  }

  /**
   * 更新配置
   */
  updateConfig(options) {
    this.config.value = { ...this.config.value, ...options }
    if (this.currentTTS) {
      this.currentTTS.config = this.config.value
    }
  }
}

// 导出单例
export const ttsManager = new TTSManager()

// 导出配置
export { ttsConfig, TTS_PROVIDERS }
