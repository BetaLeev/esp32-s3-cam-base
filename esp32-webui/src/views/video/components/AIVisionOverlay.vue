<template>
  <div
    v-if="visible"
    ref="overlayRef"
    class="ai-vision-overlay"
    :style="overlayStyle"
    @mousedown="startDrag"
    @touchstart="startDrag"
  >
    <!-- 拖拽把手 -->
    <div class="drag-handle">
      <el-icon><DCaret /></el-icon>
      <span>AI 识别</span>
      <div class="handle-actions">
        <el-button
          type="text"
          size="small"
          @click.stop="toggleExpand"
        >
          {{ expanded ? '▼' : '▶' }}
        </el-button>
        <el-button
          type="text"
          size="small"
          :icon="Close"
          @click.stop="close"
        />
      </div>
    </div>

    <!-- AI 功能开关 -->
    <div class="overlay-content" v-show="expanded">
      <div class="feature-toggles">
        <div class="toggle-row">
          <span class="toggle-label">人脸</span>
          <el-switch v-model="aiFeatures.faceDetection" size="small" />
        </div>
        <div class="toggle-row">
          <span class="toggle-label">物体</span>
          <el-switch v-model="aiFeatures.objectDetection" size="small" />
        </div>
        <div class="toggle-row">
          <span class="toggle-label">人体</span>
          <el-switch v-model="aiFeatures.personDetection" size="small" />
        </div>
        <div class="toggle-row">
          <span class="toggle-label">二维码</span>
          <el-switch v-model="aiFeatures.qrCode" size="small" />
        </div>
      </div>

      <!-- 识别结果 -->
      <div class="overlay-results" v-if="hasAnyResult">
        <!-- 人脸结果 -->
        <div v-if="aiFeatures.faceDetection && faceResults.length > 0" class="result-row">
          <el-icon><User /></el-icon>
          <span>人脸 x{{ faceResults.length }}</span>
        </div>

        <!-- 物体结果 -->
        <div v-if="aiFeatures.objectDetection && objectResults.length > 0" class="result-row">
          <el-icon><Box /></el-icon>
          <div class="object-tags">
            <el-tag
              v-for="(obj, index) in objectResults.slice(0, 3)"
              :key="index"
              size="small"
              type="info"
            >
              {{ obj.label }}
            </el-tag>
          </div>
        </div>

        <!-- 人体结果 -->
        <div v-if="aiFeatures.personDetection && personCount > 0" class="result-row">
          <el-icon><UserFilled /></el-icon>
          <span>人体 x{{ personCount }}</span>
        </div>

        <!-- 二维码结果 -->
        <div v-if="aiFeatures.qrCode && qrCodeResult" class="result-row qr-result">
          <el-icon><Link /></el-icon>
          <span class="qr-text">{{ qrCodeResult }}</span>
        </div>
      </div>

      <div v-else class="no-results">
        <span>未检测到目标</span>
      </div>
    </div>
  </div>
</template>

<script setup>
import { ref, computed, watch, onMounted, onUnmounted } from 'vue'
import { Close, DCaret, User, Box, UserFilled, Link } from '@element-plus/icons-vue'

const props = defineProps({
  visible: { type: Boolean, default: false },
  isStreaming: { type: Boolean, default: false }
})

const emit = defineEmits(['close'])

// 位置和大小
const overlayRef = ref(null)
const expanded = ref(true)

// 位置状态
const position = ref({ x: 20, y: 20 })
const size = ref({ width: 200, height: 100 })

// 拖拽状态
const isDragging = ref(false)
const dragOffset = ref({ x: 0, y: 0 })

// AI 功能开关
const aiFeatures = ref({
  faceDetection: false,
  objectDetection: false,
  personDetection: false,
  qrCode: false
})

// 识别结果（模拟数据）
const faceResults = ref([])
const objectResults = ref([])
const personCount = ref(0)
const qrCodeResult = ref('')

// 是否有结果
const hasAnyResult = computed(() => {
  return faceResults.value.length > 0 ||
         objectResults.value.length > 0 ||
         personCount.value > 0 ||
         qrCodeResult.value
})

// 计算样式
const overlayStyle = computed(() => ({
  position: 'fixed',
  left: `${position.value.x}px`,
  top: `${position.value.y}px`,
  width: expanded.value ? `${size.value.width}px` : 'auto',
  zIndex: 2000
}))

// 展开/收起
const toggleExpand = () => {
  expanded.value = !expanded.value
}

// 关闭
const close = () => {
  emit('close')
}

// 开始拖拽
const startDrag = (e) => {
  e.preventDefault()
  isDragging.value = true

  const clientX = e.type === 'touchstart' ? e.touches[0].clientX : e.clientX
  const clientY = e.type === 'touchstart' ? e.touches[0].clientY : e.clientY

  dragOffset.value = {
    x: clientX - position.value.x,
    y: clientY - position.value.y
  }

  document.addEventListener('mousemove', onDrag)
  document.addEventListener('mouseup', stopDrag)
  document.addEventListener('touchmove', onDrag)
  document.addEventListener('touchend', stopDrag)
}

// 拖拽中
const onDrag = (e) => {
  if (!isDragging.value) return

  const clientX = e.type === 'touchmove' ? e.touches[0].clientX : e.clientX
  const clientY = e.type === 'touchmove' ? e.touches[0].clientY : e.clientY

  // 边界检测
  const maxX = window.innerWidth - (overlayRef.value?.offsetWidth || 200)
  const maxY = window.innerHeight - (overlayRef.value?.offsetHeight || 100)

  position.value = {
    x: Math.max(0, Math.min(maxX, clientX - dragOffset.value.x)),
    y: Math.max(0, Math.min(maxY, clientY - dragOffset.value.y))
  }
}

// 停止拖拽
const stopDrag = () => {
  isDragging.value = false
  document.removeEventListener('mousemove', onDrag)
  document.removeEventListener('mouseup', stopDrag)
  document.removeEventListener('touchmove', onDrag)
  document.removeEventListener('touchend', stopDrag)
}

// 模拟数据更新
let mockInterval = null

const updateMockResults = () => {
  if (aiFeatures.value.faceDetection) {
    faceResults.value = Math.random() > 0.7
      ? [{ x: 100, y: 150 }, { x: 300, y: 200 }]
      : []
  }

  if (aiFeatures.value.objectDetection) {
    objectResults.value = Math.random() > 0.6
      ? [
          { label: 'person', confidence: 0.95 },
          { label: 'bottle', confidence: 0.82 }
        ]
      : []
  }

  if (aiFeatures.value.personDetection) {
    personCount.value = Math.random() > 0.7 ? Math.floor(Math.random() * 3) + 1 : 0
  }

  if (aiFeatures.value.qrCode) {
    qrCodeResult.value = Math.random() > 0.9 ? 'https://esp32.cn' : ''
  }
}

// 监听视频流状态
watch(() => props.isStreaming, (streaming) => {
  if (streaming && props.visible) {
    // 开始模拟更新
    if (!mockInterval) {
      mockInterval = setInterval(updateMockResults, 1500)
    }
  } else {
    // 停止模拟更新
    if (mockInterval) {
      clearInterval(mockInterval)
      mockInterval = null
    }
    // 清空结果
    faceResults.value = []
    objectResults.value = []
    personCount.value = 0
    qrCodeResult.value = ''
  }
})

// 监听可见性
watch(() => props.visible, (visible) => {
  if (!visible && mockInterval) {
    clearInterval(mockInterval)
    mockInterval = null
  }
})

onUnmounted(() => {
  if (mockInterval) {
    clearInterval(mockInterval)
    mockInterval = null
  }
  stopDrag()
})
</script>

<style scoped>
.ai-vision-overlay {
  position: fixed;
  z-index: 1000;
  background: rgba(30, 30, 40, 0.95);
  border-radius: 12px;
  box-shadow: 0 4px 20px rgba(0, 0, 0, 0.4);
  backdrop-filter: blur(10px);
  border: 1px solid rgba(255, 255, 255, 0.1);
  color: #fff;
  font-size: 12px;
  overflow: hidden;
  cursor: move;
  user-select: none;
  min-width: 120px;
}

.drag-handle {
  display: flex;
  align-items: center;
  gap: 6px;
  padding: 8px 12px;
  background: rgba(64, 158, 255, 0.3);
  cursor: move;
  font-weight: 500;
}

.drag-handle span {
  flex: 1;
}

.handle-actions {
  display: flex;
  gap: 2px;
}

.handle-actions .el-button {
  color: rgba(255, 255, 255, 0.7);
  padding: 2px;
  min-height: auto;
}

.handle-actions .el-button:hover {
  color: #fff;
}

.overlay-content {
  padding: 10px;
}

.feature-toggles {
  display: grid;
  grid-template-columns: repeat(2, 1fr);
  gap: 6px;
  margin-bottom: 10px;
}

.toggle-row {
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding: 4px 8px;
  background: rgba(255, 255, 255, 0.05);
  border-radius: 4px;
}

.toggle-label {
  font-size: 11px;
  color: rgba(255, 255, 255, 0.8);
}

.overlay-results {
  display: flex;
  flex-direction: column;
  gap: 6px;
}

.result-row {
  display: flex;
  align-items: center;
  gap: 6px;
  padding: 4px 8px;
  background: rgba(103, 194, 58, 0.2);
  border-radius: 4px;
  font-size: 11px;
}

.result-row .el-icon {
  color: #67c23a;
}

.object-tags {
  display: flex;
  gap: 4px;
  flex-wrap: wrap;
}

.object-tags .el-tag {
  font-size: 10px;
  padding: 0 4px;
  height: 18px;
}

.qr-result {
  background: rgba(64, 158, 255, 0.2);
}

.qr-text {
  flex: 1;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.no-results {
  text-align: center;
  color: rgba(255, 255, 255, 0.4);
  padding: 8px;
  font-size: 11px;
}
</style>
