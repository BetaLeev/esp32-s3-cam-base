/**
 * @file llm.js
 * @brief LLM (Large Language Model) 大语言模型模块
 *
 * 支持多种 LLM 服务：
 * - OpenAI (GPT)
 * - 硅基流动 (DeepSeek/通义)
 * - 智谱 AI (GLM)
 * - 月之暗面 (Kimi)
 * - 百度 (文心一言)
 * - 讯飞 (星火)
 */

import { ref } from 'vue'

// ========================================
// LLM 提供商配置
// ========================================

export const LLM_PROVIDERS = {
  OPENAI: 'openai',            // OpenAI GPT
  SILICONFLOW: 'siliconflow',  // 硅基流动
  ZHIPU: 'zhipu',              // 智谱 AI
  MOONSHOT: 'moonshot',        // 月之暗面 (Kimi)
  BAIDU: 'baidu',              // 百度文心一言
  XUNFEI: 'xunfei',           // 讯飞星火
  DEEPSEEK: 'deepseek',         // DeepSeek
 QWEN: 'qwen',               // 通义千问
  OLLAMA: 'ollama',             // Ollama 本地部署
}

// LLM 配置
const llmConfig = ref({
  provider: '',                      // 当前提供商
  apiKey: '',                        // API Key
  baseUrl: '',                       // API 地址
  model: '',                         // 模型名称
  temperature: 0.7,                  // 创造性参数
  maxTokens: 2000,                   // 最大 token 数
  timeout: 30000,                    // 超时时间(ms)
  systemPrompt: '你是一个智能助手，名字叫小智。',  // 系统提示词
})

// ========================================
// 提供商默认配置
// ========================================

export const LLM_PROVIDER_CONFIG = {
  [LLM_PROVIDERS.OPENAI]: {
    name: 'OpenAI',
    baseUrl: 'https://api.openai.com/v1',
    models: ['gpt-3.5-turbo', 'gpt-4', 'gpt-4-turbo'],
    defaultModel: 'gpt-3.5-turbo',
    docs: 'https://platform.openai.com/docs/'
  },
  [LLM_PROVIDERS.SILICONFLOW]: {
    name: '硅基流动',
    baseUrl: 'https://api.siliconflow.cn/v1',
    models: ['deepseek-ai/DeepSeek-V3', 'deepseek-ai/DeepSeek-R1', 'Qwen/Qwen2.5-72B-Instruct', 'Qwen/Qwen2.5-7B-Instruct'],
    defaultModel: 'deepseek-ai/DeepSeek-V3',
    docs: 'https://docs.siliconflow.cn/'
  },
  [LLM_PROVIDERS.DEEPSEEK]: {
    name: 'DeepSeek',
    baseUrl: 'https://api.deepseek.com/v1',
    models: ['deepseek-chat', 'deepseek-coder'],
    defaultModel: 'deepseek-chat',
    docs: 'https://platform.deepseek.com/'
  },
  [LLM_PROVIDERS.QWEN]: {
    name: '通义千问',
    baseUrl: 'https://dashscope.aliyuncs.com/compatible-mode/v1',
    models: ['qwen-turbo', 'qwen-plus', 'qwen-max'],
    defaultModel: 'qwen-turbo',
    docs: 'https://help.aliyun.com/document_detail/26'
  },
  [LLM_PROVIDERS.ZHIPU]: {
    name: '智谱 AI',
    baseUrl: 'https://open.bigmodel.cn/api/paas/v4',
    models: ['glm-4', 'glm-4-flash', 'glm-3-turbo'],
    defaultModel: 'glm-4-flash',
    docs: 'https://open.bigmodel.cn/dev/api'
  },
  [LLM_PROVIDERS.MOONSHOT]: {
    name: '月之暗面 (Kimi)',
    baseUrl: 'https://api.moonshot.cn/v1',
    models: ['moonshot-v1-8k', 'moonshot-v1-32k', 'moonshot-v1-128k'],
    defaultModel: 'moonshot-v1-8k',
    docs: 'https://platform.moonshot.cn/'
  },
  [LLM_PROVIDERS.BAIDU]: {
    name: '百度文心一言',
    baseUrl: 'https://qianfan.baidubce.com/v3',
    models: ['ernie-bot', 'ernie-bot-turbo', 'ernie-bot-4'],
    defaultModel: 'ernie-bot-turbo',
    docs: 'https://cloud.baidu.com/doc/WENXINWORKSHOP
  },
  [LLM_PROVIDERS.XUNFEI]: {
    name: '讯飞星火',
    baseUrl: 'https://spark-api.xf-yun.com/v3.1/chat',
    models: ['generalv3', 'generalv3.5'],
    defaultModel: 'generalv3.5',
    docs: 'https://www.xfyun.cn/doc/tts/online_tts/API.html'
  },
  [LLM_PROVIDERS.OLLAMA]: {
    name: 'Ollama 本地部署',
    baseUrl: 'http://localhost:11434/v1',
    models: ['llama3', 'qwen', 'deepseek-coder'],
    defaultModel: 'llama3',
    docs: 'https://github.com/ollama/ollama'
  },
}

// ========================================
// LLM 管理器
// ========================================

class LLMManager {
  constructor() {
    this.config = llmConfig
    this.conversationHistory = []
  }

  /**
   * 配置 LLM
   * @param {string} provider - 提供商
   * @param {object} options - 配置选项 { apiKey, model, ... }
   */
  configure(provider, options = {}) {
    const providerConfig = LLM_PROVIDER_CONFIG[provider]
    if (!providerConfig) {
      throw new Error(`不支持的 LLM 提供商: ${provider}`)
    }

    this.config.value = {
      ...this.config.value,
      provider,
      baseUrl: providerConfig.baseUrl,
      model: options.model || providerConfig.defaultModel,
      ...options
    }

    // 清除历史对话
    this.conversationHistory = []

    console.log(`[LLM] 已配置: ${providerConfig.name}`)
  }

  /**
   * 发送对话
   * @param {string} userMessage - 用户消息
   * @returns {Promise<string>} LLM 回复
   */
  async chat(userMessage) {
    if (!this.config.value.apiKey) {
      throw new Error('LLM API Key 未配置')
    }

    // 添加用户消息到历史
    this.conversationHistory.push({
      role: 'user',
      content: userMessage
    })

    try {
      const response = await this._callLLM()

      // 添加助手回复到历史
      this.conversationHistory.push({
        role: 'assistant',
        content: response
      })

      return response
    } catch (error) {
      // 移除失败的对话
      this.conversationHistory.pop()
      throw error
    }
  }

  /**
   * 调用 LLM API
   */
  async _callLLM() {
    const { baseUrl, apiKey, model, temperature, maxTokens, systemPrompt } = this.config.value

    const messages = []
    if (systemPrompt) {
      messages.push({ role: 'system', content: systemPrompt })
    }
    messages.push(...this.conversationHistory)

    const controller = new AbortController()
    const timeout = setTimeout(() => controller.abort(), this.config.value.timeout)

    try {
      const response = await fetch(`${baseUrl}/chat/completions`, {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
          'Authorization': `Bearer ${apiKey}`
        },
        body: JSON.stringify({
          model,
          messages,
          temperature,
          max_tokens: maxTokens
        }),
        signal: controller.signal
      })

      clearTimeout(timeout)

      if (!response.ok) {
        const error = await response.json().catch(() => ({}))
        throw new Error(error.error?.message || `请求失败: ${response.status}`)
      }

      const data = await response.json()
      return data.choices[0].message.content
    } catch (error) {
      clearTimeout(timeout)
      throw error
    }
  }

  /**
   * 流式对话
   * @param {string} userMessage - 用户消息
   * @param {function} onChunk - 接收片段回调
   */
  async chatStream(userMessage, onChunk) {
    if (!this.config.value.apiKey) {
      throw new Error('LLM API Key 未配置')
    }

    this.conversationHistory.push({
      role: 'user',
      content: userMessage
    })

    const { baseUrl, apiKey, model, temperature, maxTokens, systemPrompt } = this.config.value

    const messages = []
    if (systemPrompt) {
      messages.push({ role: 'system', content: systemPrompt })
    }
    messages.push(...this.conversationHistory)

    try {
      const response = await fetch(`${baseUrl}/chat/completions`, {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
          'Authorization': `Bearer ${apiKey}`
        },
        body: JSON.stringify({
          model,
          messages,
          temperature,
          max_tokens: maxTokens,
          stream: true
        })
      })

      if (!response.ok) {
        const error = await response.json().catch(() => ({}))
        throw new Error(error.error?.message || `请求失败: ${response.status}`)
      }

      const reader = response.body.getReader()
      const decoder = new TextDecoder()
      let fullContent = ''

      while (true) {
        const { done, value } = await reader.read()
        if (done) break

        const chunk = decoder.decode(value)
        const lines = chunk.split('\n')

        for (const line of lines) {
          if (line.startsWith('data: ')) {
            const data = line.slice(6)
            if (data === '[DONE]') continue

            try {
              const json = JSON.parse(data)
              const content = json.choices?.[0]?.delta?.content
              if (content) {
                fullContent += content
                onChunk?.(content)
              }
            } catch (e) {
              // 忽略解析错误
            }
          }
        }
      }

      this.conversationHistory.push({
        role: 'assistant',
        content: fullContent
      })

      return fullContent
    } catch (error) {
      this.conversationHistory.pop()
      throw error
    }
  }

  /**
   * 清除对话历史
   */
  clearHistory() {
    this.conversationHistory = []
  }

  /**
   * 获取提供商列表
   */
  static getProviders() {
    return Object.entries(LLM_PROVIDER_CONFIG).map(([key, config]) => ({
      id: key,
      name: config.name,
      models: config.models,
      docs: config.docs
    }))
  }
}

// 导出单例
export const llmManager = new LLMManager()

// 导出配置
export { llmConfig, LLM_PROVIDER_CONFIG }
