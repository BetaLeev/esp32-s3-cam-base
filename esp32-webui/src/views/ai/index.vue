<template>
  <div class="ai-chat-view">
    <!-- 聊天区域 -->
    <el-card class="chat-card" shadow="never">
      <template #header>
        <div class="chat-header">
          <div class="header-info">
            <el-avatar :size="36" class="ai-avatar">
              <el-icon :size="20"><MagicStick /></el-icon>
            </el-avatar>
            <div class="header-text">
              <span class="ai-name">ESP32 智能助手</span>
              <span class="ai-status">
                <span class="status-dot" :class="connectionStatus === 'connected' ? 'online' : 'offline'"></span>
                {{ statusText }}
              </span>
            </div>
          </div>
          <div class="header-actions">
            <!-- 测试模式开关（开发调试用） -->
            <el-tooltip content="本地测试模式（无需后端）" placement="bottom">
              <el-switch
                v-model="testMode"
                size="small"
                inline-prompt
                active-text="测试"
                inactive-text="测试"
                @change="handleTestModeChange"
              />
            </el-tooltip>

            <!-- TTS 语音开关 -->
            <el-tooltip content="语音播报开关" placement="bottom">
              <el-button
                size="small"
                text
                :type="ttsEnabled ? 'primary' : 'default'"
                @click="toggleTTS"
              >
                <el-icon><Microphone /></el-icon>
                <span v-if="!ttsEnabled" style="opacity: 0.5;">🔇</span>
                <span v-else>🔊</span>
              </el-button>
            </el-tooltip>

            <!-- TTS 测试按钮 -->
            <el-tooltip content="测试 TTS 播报" placement="bottom">
              <el-button size="small" text type="success" @click="testTTS">
                播放测试
              </el-button>
            </el-tooltip>

            <!-- 语音风格选择 -->
            <el-dropdown @command="handleVoiceStyleChange" trigger="click">
              <el-button size="small" text>
                {{ currentVoiceStyle.label }}
                <el-icon class="el-icon--right"><ArrowDown /></el-icon>
              </el-button>
              <template #dropdown>
                <el-dropdown-menu>
                  <el-dropdown-item
                    v-for="style in voiceStyles"
                    :key="style.value"
                    :command="style.value"
                    :disabled="style.disabled"
                  >
                    <div class="voice-style-item">
                      <span>{{ style.label }}</span>
                      <span class="voice-style-desc">{{ style.desc }}</span>
                    </div>
                  </el-dropdown-item>
                </el-dropdown-menu>
              </template>
            </el-dropdown>

            <!-- 语音模式切换 -->
            <el-tooltip content="切换到语音模式" placement="bottom">
              <el-button
                size="small"
                text
                :type="voiceMode ? 'primary' : 'default'"
                @click="toggleVoiceMode"
              >
                <el-icon><Microphone /></el-icon>
              </el-button>
            </el-tooltip>
            <el-button size="small" text @click="clearChat">
              <el-icon><Delete /></el-icon>
              清空
            </el-button>
          </div>
        </div>
      </template>

      <!-- 消息列表 -->
      <div class="chat-messages" ref="messagesRef">
        <!-- 欢迎消息 -->
        <div v-if="messages.length === 0 && !voiceMode" class="welcome-message">
          <div class="welcome-icon">
            <el-icon :size="48"><ChatDotRound /></el-icon>
          </div>
          <h3>你好！我是 ESP32 智能助手</h3>
          <p>我可以帮你：</p>
          <ul>
            <li>查询设备状态和传感器数据</li>
            <li>控制执行器（水泵、舵机、LED等）</li>
            <li>解答 ESP32 开发相关问题</li>
            <li>提供代码建议和调试帮助</li>
          </ul>
          <p class="tip">点击右上角麦克风图标开始语音对话</p>
        </div>

        <!-- 消息列表 -->
        <div
          v-for="(msg, index) in messages"
          :key="index"
          class="message-item"
          :class="msg.role"
        >
          <!-- AI 消息 -->
          <template v-if="msg.role === 'assistant'">
            <el-avatar :size="32" class="message-avatar ai">
              <el-icon><MagicStick /></el-icon>
            </el-avatar>
            <div class="message-content ai-content">
              <div class="message-bubble ai-bubble">
                <div v-if="msg.loading" class="typing-indicator">
                  <span></span><span></span><span></span>
                </div>
                <template v-else>{{ msg.content }}</template>
              </div>
              <div class="message-time">{{ msg.time }}</div>
            </div>
          </template>

          <!-- 用户消息 -->
          <template v-else>
            <div class="message-content user-content">
              <div class="message-bubble user-bubble">
                {{ msg.content }}
                <el-icon v-if="msg.isVoice" class="voice-icon"><Microphone /></el-icon>
              </div>
              <div class="message-time">{{ msg.time }}</div>
            </div>
            <el-avatar :size="32" class="message-avatar user">
              <template v-if="msg.isVoice"><el-icon><Microphone /></el-icon></template>
              <template v-else>U</template>
            </el-avatar>
          </template>
        </div>

        <!-- 语音模式提示 -->
        <div v-if="voiceMode && messages.length === 0" class="voice-hint">
          <div class="voice-hint-icon" :class="{ recording: isRecording }">
            <el-icon :size="64"><Microphone /></el-icon>
          </div>
          <p v-if="!isRecording && !isProcessing">点击下方按钮开始说话</p>
          <p v-if="isRecording" class="recording-text">
            <span class="pulse-dot"></span>
            正在聆听...
          </p>
          <p v-if="isProcessing" class="processing-text">
            <el-icon class="is-loading"><Loading /></el-icon>
            正在识别...
          </p>
        </div>
      </div>

      <!-- 输入区域 -->
      <div class="chat-input-area">
        <!-- 语音模式：按住说话按钮 -->
        <div v-if="voiceMode" class="voice-input-area">
          <div class="voice-status">
            <el-tag v-if="micPermission === 'denied'" type="danger" size="small">
              麦克风权限被拒绝
              <span v-if="isIOS">（需要HTTPS访问）</span>
            </el-tag>
            <el-tag v-else-if="micPermission === 'granted'" type="success" size="small">
              麦克风就绪
            </el-tag>
            <el-tag v-else type="warning" size="small">
              点击按钮授权麦克风
            </el-tag>
          </div>

          <!-- iOS HTTPS 提示 -->
          <div v-if="isIOS && micPermission !== 'granted'" class="ios-tip">
            <el-alert type="warning" :closable="false" show-icon>
              <template #title>
                <span>iOS 设备需要使用 HTTPS 访问才能使用麦克风</span>
              </template>
              <template #default>
                <p>请使用 ngrok 或 Cloudflare Tunnel 创建 HTTPS 隧道：</p>
                <code>ngrok http http://你的ESP32_IP:80</code>
              </template>
            </el-alert>
          </div>

          <el-button
            class="voice-button"
            :class="{ recording: isRecording, disabled: !canRecord }"
            :disabled="!canRecord || isProcessing"
            circle
            type="primary"
            :size="64"
            @mousedown="startRecording"
            @mouseup="stopRecording"
            @mouseleave="handleMouseLeave"
            @touchstart.prevent="startRecording"
            @touchend.prevent="stopRecording"
          >
            <el-icon :size="32"><Microphone /></el-icon>
          </el-button>

          <div class="voice-hints">
            <span v-if="isRecording">松开结束录音</span>
            <span v-else-if="isProcessing">处理中...</span>
            <span v-else>按住说话</span>
          </div>
        </div>

        <!-- 文本模式 -->
        <div v-else class="text-input-area">
          <el-input
            v-model="inputText"
            type="textarea"
            :rows="2"
            placeholder="输入你的问题..."
            resize="none"
            :disabled="sending"
            @keydown.enter.ctrl="handleSend"
            @keydown.enter.exact="handleEnter"
          />
          <div class="input-actions">
            <span class="input-hint">按 Enter 发送，Ctrl+Enter 换行</span>
            <el-button
              type="primary"
              :loading="sending"
              @click="handleSend"
              :disabled="!inputText.trim()"
            >
              <el-icon><Promotion /></el-icon>
              发送
            </el-button>
          </div>
        </div>
      </div>
    </el-card>
  </div>
</template>

<script setup>
import { ref, computed, onMounted, onUnmounted, nextTick } from 'vue'
import { ElMessage } from 'element-plus'
import {
  Delete,
  MagicStick,
  ChatDotRound,
  Promotion,
  Microphone,
  Loading,
  ArrowDown
} from '@element-plus/icons-vue'
import {
  aiVoiceService,
  AudioRecorder,
  sendChatMessage as apiSendChatMessage,
  getAiStatus
} from '@/api/ai'

// ========================================
// 状态
// ========================================
const messages = ref([])
const inputText = ref('')
const sending = ref(false)
const messagesRef = ref(null)

// 语音相关状态
const voiceMode = ref(false)
const isRecording = ref(false)
const isProcessing = ref(false)
const micPermission = ref('prompt') // 'prompt' | 'granted' | 'denied'
const connectionStatus = ref('disconnected')
const testMode = ref(false) // 本地测试模式
const ttsEnabled = ref(true) // TTS 语音播报开关

// 语音风格选项 (浏览器 TTS)
const voiceStyles = [
  { label: '普通话', value: 'zh-CN', voice: 'zh-CN', desc: '标准普通话', disabled: false },
  { label: '台湾腔', value: 'zh-TW', voice: 'zh-TW', desc: '台湾口音', disabled: false },
  { label: '粤语', value: 'zh-HK', voice: 'zh-HK', desc: '粤语口音', disabled: false },
]

// 当前选中的语音风格
const currentVoiceStyle = ref(voiceStyles[0])

// 计算属性：获取当前语音风格的 tl 参数
const currentTl = computed(() => currentVoiceStyle.value.tl)

// 平台检测
const isIOS = ref(false)

// 音频录制器
let audioRecorder = null

// ========================================
// 计算属性
// ========================================
const statusText = computed(() => {
  switch (connectionStatus.value) {
    case 'connected':
      return testMode.value ? '测试模式' : '在线'
    case 'connecting':
      return '连接中...'
    case 'error':
      return '连接失败'
    case 'offline':
      return '离线（后端未连接）'
    default:
      return '离线'
  }
})

const canRecord = computed(() => {
  return micPermission.value === 'granted' && !isRecording.value && !isProcessing.value
})

// ========================================
// 测试模式处理
// ========================================
function handleTestModeChange(enabled) {
  aiVoiceService.setTestMode(enabled)

  // 重新启用自动重连（用于连接真实后端）
  aiVoiceService.autoReconnect = true
  aiVoiceService.reconnectAttempts = 0

  if (enabled) {
    ElMessage.success('已开启测试模式，将模拟后端响应')
  } else {
    ElMessage.info('已关闭测试模式，将连接真实后端')
  }
}

// ========================================
// TTS 语音播报控制
// ========================================
function toggleTTS() {
  ttsEnabled.value = !ttsEnabled.value
  aiVoiceService.setTTSEnabled(ttsEnabled.value)

  if (ttsEnabled.value) {
    ElMessage.success('已开启语音播报')
  } else {
    ElMessage.info('已关闭语音播报')
    aiVoiceService.stopSpeaking()
  }
}

// 语音风格切换
function handleVoiceStyleChange(styleValue) {
  const style = voiceStyles.find(s => s.value === styleValue)
  if (style) {
    currentVoiceStyle.value = style
    ElMessage.success(`已切换为: ${style.label} - ${style.desc}`)
    // 测试播放一下
    aiVoiceService.speakWithEdgeTTS('语音风格已切换', style.voice)
  }
}

// TTS 测试
function testTTS() {
  if (!aiVoiceService.isTTSSupported()) {
    ElMessage.error('当前浏览器不支持 TTS')
    return
  }

  // 测试文本
  const testTexts = [
    '你好！这是一个 TTS 测试。',
    '当前芯片温度为 50 摄氏度。',
    '已打开 LED 灯。',
    '系统运行正常，所有模块正常工作。'
  ]

  // 依次播放测试文本
  let index = 0
  const speakNext = () => {
    if (index < testTexts.length) {
      const text = testTexts[index]
      console.log(`[TTS 测试 ${index + 1}/${testTexts.length}]: ${text}`)
      ElMessage.info(`播放: ${text}`)

      // 使用 Google TTS（质量更好）
      aiVoiceService.speakWithEdgeTTS(text, currentVoiceStyle.value.voice)

      // 延迟后播放下一个
      setTimeout(() => {
        index++
        if (index < testTexts.length) {
          setTimeout(speakNext, 800)
        } else {
          ElMessage.success('TTS 测试完成！')
        }
      }, 1500)
    }
  }

  // 停止当前播放并开始
  aiVoiceService.stopSpeaking()
  speakNext()
}

// 选择最佳中文语音
function selectBestChineseVoice() {
  const voices = aiVoiceService.getVoices()
  console.log('[TTS] 可用语音列表:', voices.map(v => ({
    name: v.name,
    lang: v.lang,
    local: v.localService
  })))

  // 优先选择中文语音
  const chineseVoices = voices.filter(v => v.lang.includes('zh'))

  if (chineseVoices.length > 0) {
    // 优先选择本地服务（更稳定）
    const localVoice = chineseVoices.find(v => v.localService)
    return localVoice || chineseVoices[0]
  }

  return null
}

// ========================================
// 生命周期
// ========================================
onMounted(async () => {
  // 检测 iOS 设备
  isIOS.value = /iPad|iPhone|iPod/.test(navigator.userAgent) && !window.MSStream

  // 检查麦克风权限
  await checkMicPermission()

  // 连接 WebSocket
  setupWebSocket()

  // 初始化音频录制器
  audioRecorder = new AudioRecorder()
})

onUnmounted(() => {
  // 清理
  if (audioRecorder) {
    audioRecorder.release()
  }
  aiVoiceService.disconnect()
})

// ========================================
// WebSocket 设置
// ========================================
function setupWebSocket() {
  connectionStatus.value = 'connecting'

  aiVoiceService.onStatusChange = (status) => {
    connectionStatus.value = status
    if (status === 'error') {
      ElMessage.error('AI服务连接失败')
    }
  }

  aiVoiceService.onRecognitionResult = (data) => {
    handleRecognitionResult(data)
  }

  aiVoiceService.onError = (error) => {
    ElMessage.error(error)
  }

  aiVoiceService.connect()
}

// ========================================
// 麦克风权限
// ========================================
async function checkMicPermission() {
  try {
    const result = await navigator.permissions.query({ name: 'microphone' })
    micPermission.value = result.state

    result.onchange = () => {
      micPermission.value = result.state
    }
  } catch (err) {
    console.warn('[AI] 权限查询失败:', err)
    micPermission.value = 'prompt'
  }
}

async function requestMicPermission() {
  if (!audioRecorder) {
    audioRecorder = new AudioRecorder()
  }

  const success = await audioRecorder.init()
  micPermission.value = success ? 'granted' : 'denied'

  if (!success) {
    ElMessage.error('无法访问麦克风，请检查权限设置')
  }

  return success
}

// ========================================
// 语音模式
// ========================================
function toggleVoiceMode() {
  voiceMode.value = !voiceMode.value

  if (voiceMode.value && micPermission.value !== 'granted') {
    requestMicPermission()
  }
}

async function startRecording() {
  if (!canRecord.value) return

  // 确保麦克风已授权
  if (micPermission.value !== 'granted') {
    const granted = await requestMicPermission()
    if (!granted) return
  }

  // 记录录音开始时间
  const recordStartTime = Date.now()
  console.log(`[AI-View] 录音开始: ${new Date(recordStartTime).toISOString()}`)

  // 开始录制
  const success = audioRecorder.start()
  if (success) {
    isRecording.value = true
    aiVoiceService.sendStart()

    // 添加用户消息占位
    messages.value.push({
      role: 'user',
      content: '正在说话...',
      time: formatTime(),
      isVoice: true,
      loading: true,
      recordStartTime // 记录录音开始时间
    })

    scrollToBottom()
  }
}

async function stopRecording() {
  if (!isRecording.value) return

  // 记录停止录音时间
  const stopTime = Date.now()
  console.log(`[AI-View] 录音停止: ${new Date(stopTime).toISOString()}`)

  isRecording.value = false
  aiVoiceService.sendEnd()

  // 停止录制并获取音频
  const audioBlob = await audioRecorder.stop()

  // 记录获取音频时间
  const gotAudioTime = Date.now()
  const recordDuration = gotAudioTime - stopTime
  console.log(`[AI-View] 获取音频: +${recordDuration}ms, 大小: ${audioBlob ? audioBlob.size : 0} bytes`)

  if (audioBlob && audioBlob.size > 0) {
    // 更新最后一条消息
    const lastMsg = messages.value[messages.value.length - 1]
    if (lastMsg && lastMsg.loading) {
      lastMsg.content = '[语音消息]'
      lastMsg.loading = false
      lastMsg.recordDuration = recordDuration
    }

    // 发送到服务器
    isProcessing.value = true
    const sendStartTime = Date.now()
    console.log(`[AI-View] 开始发送音频: ${new Date(sendStartTime).toISOString()}`)

    try {
      // 发送音频数据
      aiVoiceService.sendAudio(audioBlob)

      // 记录发送完成时间
      console.log(`[AI-View] 音频已提交: +${Date.now() - sendStartTime}ms`)
    } catch (err) {
      console.error('[AI] 发送音频失败:', err)
      ElMessage.error('音频发送失败')
    } finally {
      isProcessing.value = false
    }
  }

  scrollToBottom()
}

function handleMouseLeave() {
  if (isRecording.value) {
    stopRecording()
  }
}

// ========================================
// 文本消息
// ========================================
async function handleSend() {
  const text = inputText.value.trim()
  if (!text || sending.value) return

  // 添加用户消息
  messages.value.push({
    role: 'user',
    content: text,
    time: formatTime(),
    isVoice: false
  })

  inputText.value = ''
  sending.value = true
  scrollToBottom()

  // 添加AI占位消息
  const aiMsgIndex = messages.value.length
  messages.value.push({
    role: 'assistant',
    content: '',
    time: formatTime(),
    loading: true
  })

  try {
    // 通过 WebSocket 发送文本
    if (connectionStatus.value === 'connected') {
      aiVoiceService.sendText(text)
    } else {
      // 降级到 HTTP API
      const response = await apiSendChatMessage(text)
      if (aiMsgIndex < messages.value.length) {
        messages.value[aiMsgIndex].content = response
        messages.value[aiMsgIndex].loading = false

        // HTTP API 响应也需要 TTS
        if (ttsEnabled.value && response) {
          aiVoiceService.speakWithEdgeTTS(response, currentVoiceStyle.value.voice)
        }
      }
    }
  } catch (error) {
    console.error('Chat error:', error)
    if (aiMsgIndex < messages.value.length) {
      messages.value[aiMsgIndex].content = '抱歉，服务暂时不可用，请稍后重试。'
      messages.value[aiMsgIndex].loading = false
    }
  } finally {
    sending.value = false
    scrollToBottom()
  }
}

function handleEnter(e) {
  // Enter 发送消息，阻止默认换行行为
  // Shift+Enter 或 Ctrl+Enter 才换行
  if (e.key === 'Enter' && !e.shiftKey && !e.ctrlKey && !e.metaKey) {
    e.preventDefault()
    handleSend()
  }
}

// ========================================
// 识别结果处理
// ========================================
function handleRecognitionResult(data) {
  // 记录收到响应时间
  const responseTime = Date.now()
  console.log(`[AI-View] 收到识别结果: ${new Date(responseTime).toISOString()}`)

  // 计算端到端延迟
  const lastMsg = messages.value.find(m => m.role === 'user' && m.isVoice && m.recordStartTime)
  if (lastMsg) {
    const totalDelay = responseTime - lastMsg.recordStartTime
    console.log(`[AI-View] ===== 端到端延迟统计 =====`)
    console.log(`[AI-View] 录音时长: ${lastMsg.recordDuration || '?'}ms`)
    console.log(`[AI-View] 总延迟: ${totalDelay}ms (${(totalDelay / 1000).toFixed(2)}s)`)
    console.log(`[AI-View] 命令: ${data.text || data.command}`)
    console.log(`[AI-View] 响应: ${data.action || data.reply}`)
  }

  // 获取 AI 回复文本
  const aiResponse = data.action || data.reply || ''

  // 添加用户消息
  messages.value.push({
    role: 'user',
    content: data.text || data.command || '[无法识别]',
    time: formatTime(),
    isVoice: true
  })

  // 添加 AI 回复
  if (aiResponse) {
    messages.value.push({
      role: 'assistant',
      content: aiResponse,
      time: formatTime(),
      loading: false
    })

    // TTS 自动播放（使用 Edge TTS）
    if (ttsEnabled.value && aiResponse) {
      aiVoiceService.speakWithEdgeTTS(aiResponse, currentVoiceStyle.value.voice)
    }
  }

  isProcessing.value = false
  scrollToBottom()
}

// ========================================
// 工具函数
// ========================================
function formatTime() {
  const now = new Date()
  return `${now.getHours().toString().padStart(2, '0')}:${now.getMinutes().toString().padStart(2, '0')}`
}

function scrollToBottom() {
  nextTick(() => {
    if (messagesRef.value) {
      messagesRef.value.scrollTop = messagesRef.value.scrollHeight
    }
  })
}

function clearChat() {
  messages.value = []
  ElMessage.success('对话已清空')
}
</script>

<style scoped>
.ai-chat-view {
  height: calc(100vh - 140px);
  display: flex;
  flex-direction: column;
}

.chat-card {
  flex: 1;
  display: flex;
  flex-direction: column;
  overflow: hidden;
}

.chat-card :deep(.el-card__header) {
  padding: 12px 16px;
  border-bottom: 1px solid #ebeef5;
}

.chat-card :deep(.el-card__body) {
  flex: 1;
  display: flex;
  flex-direction: column;
  padding: 0;
  overflow: hidden;
}

/* 头部 */
.chat-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
}

.header-info {
  display: flex;
  align-items: center;
  gap: 12px;
}

.ai-avatar {
  background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
  color: #fff;
}

.header-text {
  display: flex;
  flex-direction: column;
}

.ai-name {
  font-weight: 600;
  font-size: 15px;
  color: #303133;
}

.ai-status {
  display: flex;
  align-items: center;
  gap: 4px;
  font-size: 12px;
  color: #909399;
}

.status-dot {
  width: 6px;
  height: 6px;
  border-radius: 50%;
}

.status-dot.online {
  background: #67c23a;
}

.status-dot.offline {
  background: #909399;
}

/* 消息区域 */
.chat-messages {
  flex: 1;
  overflow-y: auto;
  padding: 16px;
  scroll-behavior: smooth;
}

/* 欢迎消息 */
.welcome-message {
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  height: 100%;
  text-align: center;
  color: #606266;
}

.welcome-icon {
  color: #409eff;
  margin-bottom: 16px;
}

.welcome-message h3 {
  margin: 0 0 12px;
  color: #303133;
}

.welcome-message p {
  margin: 0 0 8px;
}

.welcome-message ul {
  text-align: left;
  list-style: none;
  padding: 0;
  margin: 0 0 16px;
}

.welcome-message li {
  padding: 4px 0;
}

.welcome-message .tip {
  font-size: 13px;
  color: #909399;
  margin-top: 16px;
}

/* 消息项 */
.message-item {
  display: flex;
  gap: 12px;
  margin-bottom: 16px;
  animation: fadeIn 0.3s ease;
}

@keyframes fadeIn {
  from {
    opacity: 0;
    transform: translateY(10px);
  }
  to {
    opacity: 1;
    transform: translateY(0);
  }
}

.message-item.user {
  flex-direction: row-reverse;
}

.message-avatar {
  flex-shrink: 0;
}

.message-avatar.ai {
  background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
  color: #fff;
}

.message-avatar.user {
  background: #409eff;
  color: #fff;
}

.message-content {
  max-width: 70%;
  display: flex;
  flex-direction: column;
}

.message-content.user-content {
  align-items: flex-end;
}

.message-bubble {
  padding: 10px 14px;
  border-radius: 12px;
  line-height: 1.5;
  font-size: 14px;
  display: flex;
  align-items: center;
  gap: 6px;
}

.ai-bubble {
  background: #f4f4f5;
  color: #303133;
  border-top-left-radius: 4px;
}

.user-bubble {
  background: #409eff;
  color: #fff;
  border-top-right-radius: 4px;
}

.voice-icon {
  font-size: 14px;
}

.message-time {
  font-size: 11px;
  color: #c0c4cc;
  margin-top: 4px;
  padding: 0 4px;
}

/* 加载动画 */
.typing-indicator {
  display: flex;
  gap: 4px;
  padding: 4px 0;
}

.typing-indicator span {
  width: 8px;
  height: 8px;
  background: #909399;
  border-radius: 50%;
  animation: bounce 1.4s infinite ease-in-out both;
}

.typing-indicator span:nth-child(1) {
  animation-delay: -0.32s;
}

.typing-indicator span:nth-child(2) {
  animation-delay: -0.16s;
}

@keyframes bounce {
  0%, 80%, 100% {
    transform: scale(0);
  }
  40% {
    transform: scale(1);
  }
}

/* 语音模式提示 */
.voice-hint {
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  height: 100%;
  text-align: center;
  color: #606266;
}

.voice-hint-icon {
  color: #c0c4cc;
  margin-bottom: 16px;
  transition: all 0.3s ease;
}

.voice-hint-icon.recording {
  color: #409eff;
  animation: pulse 1s infinite;
}

@keyframes pulse {
  0%, 100% {
    transform: scale(1);
    opacity: 1;
  }
  50% {
    transform: scale(1.1);
    opacity: 0.8;
  }
}

.recording-text {
  color: #409eff;
  display: flex;
  align-items: center;
  gap: 8px;
}

.pulse-dot {
  width: 8px;
  height: 8px;
  background: #409eff;
  border-radius: 50%;
  animation: pulse-dot 1s infinite;
}

@keyframes pulse-dot {
  0%, 100% {
    opacity: 1;
  }
  50% {
    opacity: 0.3;
  }
}

.processing-text {
  color: #909399;
  display: flex;
  align-items: center;
  gap: 8px;
}

/* 输入区域 */
.chat-input-area {
  border-top: 1px solid #ebeef5;
  background: #fafafa;
}

/* 文本输入模式 */
.text-input-area {
  padding: 12px 16px;
}

.text-input-area :deep(.el-textarea__inner) {
  border-radius: 8px;
  font-size: 14px;
}

.input-actions {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin-top: 8px;
}

.input-hint {
  font-size: 12px;
  color: #c0c4cc;
}

/* 语音输入模式 */
.voice-input-area {
  padding: 20px 16px;
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 12px;
}

.voice-status {
  margin-bottom: 4px;
}

.voice-button {
  width: 64px;
  height: 64px;
  border-radius: 50%;
  font-size: 24px;
  transition: all 0.2s ease;
  box-shadow: 0 4px 12px rgba(64, 158, 255, 0.3);
}

.voice-button:hover:not(.disabled) {
  transform: scale(1.05);
  box-shadow: 0 6px 16px rgba(64, 158, 255, 0.4);
}

.voice-button.recording {
  background: #f56c6c;
  border-color: #f56c6c;
  box-shadow: 0 4px 12px rgba(245, 108, 108, 0.4);
  animation: pulse-button 1s infinite;
}

.voice-button.disabled {
  opacity: 0.5;
  cursor: not-allowed;
}

@keyframes pulse-button {
  0%, 100% {
    transform: scale(1);
  }
  50% {
    transform: scale(1.05);
  }
}

.voice-hints {
  font-size: 13px;
  color: #909399;
}

/* iOS 提示样式 */
.ios-tip {
  margin: 8px 16px 16px;
}

.ios-tip .el-alert {
  padding: 12px 16px;
}

.ios-tip code {
  display: block;
  margin-top: 8px;
  padding: 8px;
  background: #f5f5f5;
  border-radius: 4px;
  font-size: 12px;
  word-break: break-all;
}
</style>
