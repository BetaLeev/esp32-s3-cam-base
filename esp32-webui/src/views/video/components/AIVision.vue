<template>
  <el-card class="ai-vision-card">
    <template #header>
      <div class="card-header">
        <span>AI 视觉识别</span>
        <div class="header-actions">
          <el-switch
            v-model="isEnabled"
            active-text="开启"
            inactive-text="关闭"
            inline-prompt
            size="small"
            @change="onToggleEnabled"
          />
          <el-button
            type="primary"
            size="small"
            plain
            @click="openOverlay"
          >
            <el-icon><Monitor /></el-icon>
            视频叠加
          </el-button>
        </div>
      </div>
    </template>

    <!-- AI 功能开关 -->
    <div class="ai-controls">
      <el-row :gutter="12">
        <el-col :span="12">
          <div class="ai-toggle-item">
            <span class="toggle-label">人脸检测</span>
            <el-switch
              v-model="aiFeatures.faceDetection"
              :disabled="!isEnabled"
              @change="onFeatureChange('faceDetection')"
            />
          </div>
        </el-col>
        <el-col :span="12">
          <div class="ai-toggle-item">
            <span class="toggle-label">物体识别</span>
            <el-switch
              v-model="aiFeatures.objectDetection"
              :disabled="!isEnabled"
              @change="onFeatureChange('objectDetection')"
            />
          </div>
        </el-col>
        <el-col :span="12">
          <div class="ai-toggle-item">
            <span class="toggle-label">人体检测</span>
            <el-switch
              v-model="aiFeatures.personDetection"
              :disabled="!isEnabled"
              @change="onFeatureChange('personDetection')"
            />
          </div>
        </el-col>
        <el-col :span="12">
          <div class="ai-toggle-item">
            <span class="toggle-label">二维码扫描</span>
            <el-switch
              v-model="aiFeatures.qrCode"
              :disabled="!isEnabled"
              @change="onFeatureChange('qrCode')"
            />
          </div>
        </el-col>
      </el-row>
    </div>

    <!-- 识别结果展示 -->
    <div class="recognition-results" v-if="isEnabled">
      <el-divider content-position="left">识别结果</el-divider>

      <!-- 人脸检测结果 -->
      <div v-if="aiFeatures.faceDetection && faceResults.length > 0" class="result-section">
        <div class="result-title">
          <el-icon><User /></el-icon>
          <span>检测到 {{ faceResults.length }} 个人脸</span>
        </div>
        <div class="face-list">
          <div v-for="(face, index) in faceResults" :key="index" class="face-item">
            <el-tag size="small">人脸 {{ index + 1 }}</el-tag>
            <span class="face-info">坐标: ({{ face.x }}, {{ face.y }})</span>
          </div>
        </div>
      </div>

      <!-- 物体识别结果 -->
      <div v-if="aiFeatures.objectDetection && objectResults.length > 0" class="result-section">
        <div class="result-title">
          <el-icon><Box /></el-icon>
          <span>识别到 {{ objectResults.length }} 个物体</span>
        </div>
        <div class="object-list">
          <el-tag
            v-for="(obj, index) in objectResults"
            :key="index"
            type="info"
            size="small"
            class="object-tag"
          >
            {{ obj.label }} {{ Math.round(obj.confidence * 100) }}%
          </el-tag>
        </div>
      </div>

      <!-- 人体检测结果 -->
      <div v-if="aiFeatures.personDetection && personCount > 0" class="result-section">
        <div class="result-title success">
          <el-icon><UserFilled /></el-icon>
          <span>检测到 {{ personCount }} 个人体</span>
        </div>
      </div>

      <!-- 二维码扫描结果 -->
      <div v-if="aiFeatures.qrCode && qrCodeResult" class="result-section">
        <div class="result-title success">
          <el-icon><Link /></el-icon>
          <span>扫描结果</span>
        </div>
        <div class="qr-result">
          <el-input
            v-model="qrCodeResult"
            readonly
            size="small"
          >
            <template #append>
              <el-button @click="copyQrResult">复制</el-button>
            </template>
          </el-input>
        </div>
      </div>

      <!-- 无结果提示 -->
      <el-empty
        v-if="!hasAnyResult"
        :image-size="60"
        description="开启识别功能后，将在视频画面中显示识别结果"
      />
    </div>

    <!-- AI 服务配置 -->
    <div class="ai-settings">
      <el-divider content-position="left">识别模式</el-divider>

      <el-radio-group v-model="aiMode" size="small" @change="onModeChange">
        <el-radio-button value="local">
          本地识别 (ESP32)
        </el-radio-button>
        <el-radio-button value="cloud">
          云端识别 (LLM)
        </el-radio-button>
      </el-radio-group>

      <div class="mode-tip">
        <el-icon><InfoFilled /></el-icon>
        <span v-if="aiMode === 'local'">
          使用 ESP32 本地模型进行识别
        </span>
        <span v-else>
          使用云端 AI 进行识别，功能更强大但需要网络
        </span>
      </div>

      <!-- 自动开启选项 -->
      <div class="auto-start-option">
        <el-checkbox v-model="autoStartWithStream">
          视频开始后自动开启 AI 识别
        </el-checkbox>
      </div>

      <!-- ESP32 原生能力说明 -->
      <div class="capability-info">
        <el-collapse>
          <el-collapse-item title="ESP32-CAM 原生 AI 能力" name="1">
            <div class="capability-list">
              <div class="capability-item">
                <el-tag size="small" type="success">二维码</el-tag>
                <span>使用 esp-camera 内置功能，无需额外模型</span>
              </div>
              <div class="capability-item">
                <el-tag size="small" type="info">人脸检测</el-tag>
                <span>需要 esp-face 库，检测人脸位置</span>
              </div>
              <div class="capability-item">
                <el-tag size="small" type="info">物体识别</el-tag>
                <span>需要加载 MobileNet-SSD 模型 (~2MB)</span>
              </div>
              <div class="capability-item">
                <el-tag size="small" type="info">人体检测</el-tag>
                <span>专门优化的轻量模型 (~200KB)</span>
              </div>
            </div>
            <div class="capability-note">
              <el-alert
                type="info"
                :closable="false"
                show-icon
              >
                <template #title>
                  当前为模拟数据，实际功能需要 ESP32 端接入 AI 模型
                </template>
              </el-alert>
            </div>
          </el-collapse-item>
        </el-collapse>
      </div>
    </div>
  </el-card>
</template>

<script setup>
import { ref, computed } from 'vue'
import { ElMessage } from 'element-plus'
import { User, Box, UserFilled, Link, InfoFilled, Monitor } from '@element-plus/icons-vue'

// AI 功能开关
const isEnabled = ref(false)
const aiMode = ref('local')  // 'local' | 'cloud'
const autoStartWithStream = ref(false)  // 视频开始后自动开启

const aiFeatures = ref({
  faceDetection: false,    // 人脸检测
  objectDetection: false,   // 物体识别
  personDetection: false,   // 人体检测
  qrCode: false            // 二维码扫描
})

// 识别结果
const faceResults = ref([])      // 人脸检测结果
const objectResults = ref([])     // 物体识别结果
const personCount = ref(0)        // 人体数量
const qrCodeResult = ref('')      // 二维码内容

// 是否有任何结果
const hasAnyResult = computed(() => {
  return faceResults.value.length > 0 ||
         objectResults.value.length > 0 ||
         personCount.value > 0 ||
         qrCodeResult.value
})

// 切换主开关
const onToggleEnabled = (enabled) => {
  console.log('[AI Vision] 主开关:', enabled)
  if (enabled) {
    startMockUpdates()
  } else {
    stopMockUpdates()
    clearResults()
  }
}

// 打开视频叠加面板
const emit = defineEmits(['openOverlay'])

const openOverlay = () => {
  emit('openOverlay')
}

// 切换 AI 功能
const onFeatureChange = (feature) => {
  console.log('[AI Vision]', feature + ':', aiFeatures.value[feature])
  // TODO: 发送到 ESP32 开启/关闭对应功能
}

// 切换模式
const onModeChange = (mode) => {
  console.log('[AI Vision] 模式切换:', mode)
  // TODO: 切换本地/云端识别
}

// 复制二维码结果
const copyQrResult = () => {
  navigator.clipboard.writeText(qrCodeResult.value)
  ElMessage.success('已复制到剪贴板')
}

// 清除结果
const clearResults = () => {
  faceResults.value = []
  objectResults.value = []
  personCount.value = 0
  qrCodeResult.value = ''
}

// 模拟数据更新（测试用）
const updateMockResults = () => {
  if (!isEnabled.value) return

  // 模拟人脸检测
  if (aiFeatures.value.faceDetection) {
    faceResults.value = Math.random() > 0.5
      ? [
          { x: 100, y: 150 },
          { x: 300, y: 200 }
        ]
      : []
  }

  // 模拟物体识别
  if (aiFeatures.value.objectDetection) {
    objectResults.value = Math.random() > 0.3
      ? [
          { label: 'person', confidence: 0.95 },
          { label: 'bottle', confidence: 0.82 }
        ]
      : []
  }

  // 模拟人体检测
  if (aiFeatures.value.personDetection) {
    personCount.value = Math.random() > 0.5 ? Math.floor(Math.random() * 3) + 1 : 0
  }

  // 模拟二维码
  if (aiFeatures.value.qrCode) {
    qrCodeResult.value = Math.random() > 0.8 ? 'https://example.com' : ''
  }
}

// 模拟定时更新
let mockInterval = null

const startMockUpdates = () => {
  if (mockInterval) return
  mockInterval = setInterval(updateMockResults, 2000)
}

const stopMockUpdates = () => {
  if (mockInterval) {
    clearInterval(mockInterval)
    mockInterval = null
  }
}

// 暴露方法给父组件
defineExpose({
  start: () => {
    if (autoStartWithStream.value) {
      isEnabled.value = true
      startMockUpdates()
    }
  },
  stop: () => {
    isEnabled.value = false
    stopMockUpdates()
    clearResults()
  },
  enable: () => {
    isEnabled.value = true
    startMockUpdates()
  },
  disable: () => {
    isEnabled.value = false
    stopMockUpdates()
    clearResults()
  },
  isEnabled,
  autoStartWithStream
})
</script>

<style scoped>
.ai-vision-card {
  margin-top: 16px;
}

.card-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
}

.header-actions {
  display: flex;
  align-items: center;
  gap: 12px;
}

.ai-controls {
  margin-bottom: 16px;
}

.ai-toggle-item {
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding: 8px 0;
}

.toggle-label {
  font-size: 14px;
}

.recognition-results {
  min-height: 100px;
}

.result-section {
  padding: 8px 0;
}

.result-title {
  display: flex;
  align-items: center;
  gap: 8px;
  font-size: 14px;
  color: #606266;
  margin-bottom: 8px;
}

.result-title.success {
  color: #67c23a;
}

.face-list {
  display: flex;
  flex-wrap: wrap;
  gap: 8px;
}

.face-item {
  display: flex;
  align-items: center;
  gap: 8px;
  padding: 4px 8px;
  background: #f5f7fa;
  border-radius: 4px;
}

.face-info {
  font-size: 12px;
  color: #909399;
}

.object-list {
  display: flex;
  flex-wrap: wrap;
  gap: 8px;
}

.object-tag {
  font-size: 12px;
}

.qr-result {
  margin-top: 8px;
}

.ai-settings {
  margin-top: 16px;
}

.mode-tip {
  display: flex;
  align-items: center;
  gap: 8px;
  margin-top: 12px;
  padding: 8px 12px;
  background: #f4f4f5;
  border-radius: 4px;
  font-size: 12px;
  color: #909399;
}

.auto-start-option {
  margin-top: 12px;
  padding: 8px 0;
}

.capability-info {
  margin-top: 12px;
}

.capability-list {
  display: flex;
  flex-direction: column;
  gap: 8px;
  margin-bottom: 12px;
}

.capability-item {
  display: flex;
  align-items: center;
  gap: 8px;
  font-size: 13px;
}

.capability-item span {
  color: #606266;
}

.capability-note {
  margin-top: 8px;
}
</style>
