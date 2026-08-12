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
                <span class="status-dot" :class="connected ? 'online' : 'offline'"></span>
                {{ connected ? '在线' : '离线' }}
              </span>
            </div>
          </div>
          <div class="header-actions">
            <el-button size="small" text @click="clearChat">
              <el-icon><Delete /></el-icon>
              清空对话
            </el-button>
          </div>
        </div>
      </template>

      <!-- 消息列表 -->
      <div class="chat-messages" ref="messagesRef">
        <!-- 欢迎消息 -->
        <div v-if="messages.length === 0" class="welcome-message">
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
              <div class="message-bubble user-bubble">{{ msg.content }}</div>
              <div class="message-time">{{ msg.time }}</div>
            </div>
            <el-avatar :size="32" class="message-avatar user">U</el-avatar>
          </template>
        </div>
      </div>

      <!-- 输入区域 -->
      <div class="chat-input-area">
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
    </el-card>
  </div>
</template>

<script setup>
import { ref, nextTick } from 'vue'
import { Delete, MagicStick, ChatDotRound, Promotion } from '@element-plus/icons-vue'
import { sendChatMessage } from '@/api/ai'

const messages = ref([])
const inputText = ref('')
const sending = ref(false)
const connected = ref(true) // TODO: 连接状态
const messagesRef = ref(null)

// 格式化时间
const formatTime = () => {
  const now = new Date()
  return `${now.getHours().toString().padStart(2, '0')}:${now.getMinutes().toString().padStart(2, '0')}`
}

// 发送消息
const handleSend = async () => {
  const text = inputText.value.trim()
  if (!text || sending.value) return

  // 添加用户消息
  messages.value.push({
    role: 'user',
    content: text,
    time: formatTime()
  })

  inputText.value = ''
  sending.value = true
  scrollToBottom()

  // 添加AI占位消息（加载状态）
  const aiMsgIndex = messages.value.length
  messages.value.push({
    role: 'assistant',
    content: '',
    time: formatTime(),
    loading: true
  })

  try {
    const response = await sendChatMessage(text)

    // 更新AI消息
    if (aiMsgIndex < messages.value.length) {
      messages.value[aiMsgIndex].content = response
      messages.value[aiMsgIndex].loading = false
    }
  } catch (error) {
    // 显示错误
    if (aiMsgIndex < messages.value.length) {
      messages.value[aiMsgIndex].content = '抱歉，服务暂时不可用，请稍后重试。'
      messages.value[aiMsgIndex].loading = false
    }
    console.error('Chat error:', error)
  } finally {
    sending.value = false
    scrollToBottom()
  }
}

// 处理回车键
const handleEnter = (e) => {
  // 默认行为，textarea会自动换行
}

// 滚动到底部
const scrollToBottom = () => {
  nextTick(() => {
    if (messagesRef.value) {
      messagesRef.value.scrollTop = messagesRef.value.scrollHeight
    }
  })
}

// 清空对话
const clearChat = () => {
  messages.value = []
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
  margin: 0;
}

.welcome-message li {
  padding: 4px 0;
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

/* 输入区域 */
.chat-input-area {
  border-top: 1px solid #ebeef5;
  padding: 12px 16px;
  background: #fafafa;
}

.chat-input-area :deep(.el-textarea__inner) {
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
</style>
