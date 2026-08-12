<template>
  <el-card class="video-card">
    <template #header>
      <div class="card-header">
        <span class="title">视频监控</span>
        <div class="header-actions">
          <el-tag
            :type="info.initialized ? 'success' : 'danger'"
            size="small"
            effect="light"
          >
            {{ info.initialized ? '摄像头在线' : '摄像头离线' }}
          </el-tag>
          <el-tag
            v-if="info.initialized"
            :type="isStreaming ? 'success' : 'info'"
            size="small"
            effect="plain"
            style="margin-left: 8px"
          >
            {{ isStreaming ? '实时流中' : '快照模式' }}
          </el-tag>
          <el-button
            :icon="Refresh"
            circle
            size="small"
            style="margin-left: 8px"
            @click="emit('refreshInfo')"
            :loading="loading"
          />
        </div>
      </div>
    </template>

    <!-- 视频显示区域 -->
    <div class="video-container">
      <!-- 未初始化状态 -->
      <div v-if="!info.initialized" class="no-camera">
        <el-icon class="no-camera-icon" :size="80"><VideoCameraFilled /></el-icon>
        <p class="no-camera-text">摄像头未连接或初始化失败</p>
        <el-button type="primary" @click="emit('refreshInfo')">
          <el-icon><Refresh /></el-icon>
          重试
        </el-button>
      </div>

      <!-- 视频流 / 快照 -->
      <div v-else class="video-wrapper">
        <!-- 视频未开启时的占位图 -->
        <div v-if="!isStreaming && !snapshotLoaded" class="video-placeholder">
          <div class="placeholder-content">
            <el-icon class="placeholder-icon"><VideoCamera /></el-icon>
            <span class="placeholder-title">摄像头待机中</span>
            <span class="placeholder-desc">点击下方"开始视频流"按钮开启实时监控</span>
          </div>
        </div>

        <!-- 快照模式 -->
        <img
          v-if="!isStreaming && snapshotLoaded"
          :key="'snapshot-' + snapshotKey"
          :src="snapshotUrl"
          alt="Camera Snapshot"
          class="video-frame"
          @load="onSnapshotLoaded"
          @error="emit('snapshotError')"
        />

        <!-- 实时流模式 (WebSocket) -->
        <img
          v-show="isStreaming"
          ref="imgRef"
          alt="Camera Stream"
          class="video-frame"
        />

        <!-- 视频底部信息叠加 -->
        <div class="video-overlay" v-if="isStreaming || snapshotLoaded">
          <div class="overlay-left">
            <el-icon v-if="isStreaming" class="rec-icon"><VideoPlay /></el-icon>
            <span v-if="isStreaming">REC · {{ fps }} fps</span>
            <span v-else>快照 · {{ info.resolution?.width || '--' }}×{{ info.resolution?.height || '--' }}</span>
          </div>
          <div class="overlay-right">
            <el-tag size="small" effect="dark" type="info">
              {{ info.sensor || 'Unknown' }}
            </el-tag>
          </div>
        </div>
      </div>
    </div>

    <!-- 控制按钮插槽 -->
    <slot name="controls" />
  </el-card>
</template>

<script setup>
import { ref, watch, onMounted } from 'vue'
import { Refresh, VideoCameraFilled, VideoPlay, VideoCamera } from '@element-plus/icons-vue'

const props = defineProps({
  info: { type: Object, required: true },
  isStreaming: { type: Boolean, default: false },
  fps: { type: Number, default: 0 },
  loading: { type: Boolean, default: false },
  snapshotUrl: { type: String, default: '' },
  snapshotKey: { type: Number, default: 0 },
  setImgRef: { type: Function, default: null }
})

const emit = defineEmits([
  'refreshInfo',
  'snapshotLoad',
  'snapshotError'
])

// img 元素引用
const imgRef = ref(null)

// 快照是否已加载
const snapshotLoaded = ref(false)

// 快照加载成功
const onSnapshotLoaded = () => {
  snapshotLoaded.value = true
  emit('snapshotLoad')
}

// 监听快照 key 变化，重置加载状态
watch(() => props.snapshotKey, () => {
  snapshotLoaded.value = false
})

// 监听视频流状态变化
watch(() => props.isStreaming, (streaming) => {
  if (streaming) {
    snapshotLoaded.value = true
  }
})

// 关键修复：watch 监听切换实时流模式时创建的新的 img 标签
watch(imgRef, (newEl) => {
  if (newEl && typeof props.setImgRef === 'function') {
    props.setImgRef(newEl)
  }
})

onMounted(() => {
  if (typeof props.setImgRef === 'function' && imgRef.value) {
    props.setImgRef(imgRef.value)
  }
})
</script>

<style scoped>
.video-card {
  margin-bottom: 20px;
}

.card-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  gap: 8px;
}

.card-header .title {
  font-size: 16px;
  font-weight: 600;
  color: #303133;
}

.header-actions {
  display: flex;
  align-items: center;
}

.video-container {
  background: #0f1115;
  border-radius: 10px;
  overflow: hidden;
  position: relative;
  min-height: 360px;
  display: flex;
  justify-content: center;
  align-items: center;
}

.no-camera {
  text-align: center;
  padding: 50px 20px;
  color: #909399;
}

.no-camera-icon {
  color: #5a5e66;
  margin-bottom: 16px;
}

.no-camera-text {
  margin: 12px 0 20px;
  font-size: 14px;
}

.video-wrapper {
  position: relative;
  width: 100%;
}

/* 占位图样式 */
.video-placeholder {
  width: 100%;
  min-height: 400px;
  display: flex;
  flex-direction: column;
  justify-content: center;
  align-items: center;
  background: linear-gradient(135deg, #1a1a2e 0%, #16213e 50%, #0f3460 100%);
  position: relative;
  overflow: hidden;
}

.video-placeholder::before {
  content: '';
  position: absolute;
  top: 0;
  left: 0;
  right: 0;
  bottom: 0;
  background:
    radial-gradient(circle at 20% 80%, rgba(59, 130, 246, 0.1) 0%, transparent 50%),
    radial-gradient(circle at 80% 20%, rgba(139, 92, 246, 0.1) 0%, transparent 50%);
}

.placeholder-content {
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  text-align: center;
  z-index: 1;
  padding: 20px;
  gap: 16px;
}

.placeholder-icon {
  color: rgba(255, 255, 255, 0.3);
  font-size: 80px;
  line-height: 1;
}

.placeholder-title {
  color: rgba(255, 255, 255, 0.8);
  font-size: 18px;
  font-weight: 500;
  margin: 0;
  line-height: 1.5;
}

.placeholder-desc {
  color: rgba(255, 255, 255, 0.5);
  font-size: 14px;
  margin: 0;
  line-height: 1.5;
}

.video-frame {
  width: 100%;
  height: auto;
  max-height: 540px;
  object-fit: contain;
  background: #000;
  display: block;
}

.video-overlay {
  position: absolute;
  left: 0;
  right: 0;
  bottom: 0;
  padding: 10px 14px;
  display: flex;
  justify-content: space-between;
  align-items: center;
  color: #fff;
  font-size: 12px;
  background: linear-gradient(to top, rgba(0,0,0,0.6), rgba(0,0,0,0));
  pointer-events: none;
  z-index: 100;
}

.overlay-left {
  display: flex;
  align-items: center;
  gap: 6px;
}

.rec-icon {
  color: #f56c6c;
  animation: blink 1s infinite;
}

@keyframes blink {
  0%, 100% { opacity: 1; }
  50% { opacity: 0.3; }
}
</style>
