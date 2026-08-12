/**
 * AI 聊天 API
 * 后端接口就绪后替换 mock 实现
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
// Mock 实现（待后端提供接口后替换）
// ========================================

/**
 * 发送聊天消息
 * @param {string} message - 用户消息
 * @returns {Promise<string>} AI回复
 */
export const sendChatMessage = async (message) => {
  // TODO: 替换为实际API调用
  // return api.post('/ai/chat', { message })

  // Mock: 模拟AI回复
  return new Promise((resolve) => {
    setTimeout(() => {
      const responses = [
        '好的，我来帮你查询。当前系统运行正常，温度传感器读数正常。',
        '关于ESP32的LEDC功能，它支持最多16通道的PWM输出，可以用于控制LED亮度、电机速度等。',
        '水泵控制支持4个档位：关闭、低速(30%)、中速(60%)、高速(100%)。',
        '舵机角度范围是0-180度，当前默认90度（中间位置）。',
        'TF卡已挂载，可以正常读写文件。',
        '摄像头模块支持多种分辨率，当前配置为SXGA(1280x1024)。',
        'WiFi可以配置为AP模式或STA模式连接现有网络。'
      ]
      const randomResponse = responses[Math.floor(Math.random() * responses.length)]
      resolve(randomResponse)
    }, 1000 + Math.random() * 1000) // 1-2秒延迟
  })
}

/**
 * 获取AI对话历史
 * @returns {Promise<Array>} 历史消息列表
 */
export const getChatHistory = async () => {
  // TODO: 替换为实际API调用
  // return api.get('/ai/history')

  return Promise.resolve([])
}

/**
 * 清空对话历史
 * @returns {Promise}
 */
export const clearChatHistory = async () => {
  // TODO: 替换为实际API调用
  // return api.post('/ai/clear')

  return Promise.resolve()
}

/**
 * 获取AI服务状态
 * @returns {Promise<boolean>} 是否在线
 */
export const getAiStatus = async () => {
  // TODO: 替换为实际API调用
  // return api.get('/ai/status')

  return Promise.resolve(true)
}

export default api
