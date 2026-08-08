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
        <img
          v-if="!isStreaming"
          :key="snapshotKey"
          :src="snapshotUrl"
          alt="Camera Snapshot"
          class="video-frame"
          @load="emit('snapshotLoad')"
          @error="emit('snapshotError')"
        />
        <img
          v-else
          :src="streamUrl"
          alt="Camera Stream"
          class="video-frame"
          @load="emit('streamLoad')"
          @error="emit('streamError')"
        />

        <!-- 视频底部信息叠加 -->
        <div class="video-overlay">
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

    <!-- 控制按钮插槽（由 index.vue 通过具名插槽填充） -->
    <slot name="controls" />
  </el-card>
</template>

<script setup>
import { Refresh, VideoCameraFilled, VideoPlay } from '@element-plus/icons-vue'

defineProps({
  // 摄像头信息
  info: { type: Object, required: true },
  // 是否处于实时流模式
  isStreaming: { type: Boolean, default: false },
  // 当前 FPS
  fps: { type: Number, default: 0 },
  // 信息刷新加载态
  loading: { type: Boolean, default: false },
  // 实时流 URL
  streamUrl: { type: String, default: '' },
  // 快照 URL
  snapshotUrl: { type: String, default: '' },
  // 快照 img 强制刷新 key
  snapshotKey: { type: Number, default: 0 }
})

const emit = defineEmits([
  'refreshInfo',
  'snapshotLoad',
  'snapshotError',
  'streamLoad',
  'streamError'
])
</script>

<style scoped>
.video-card {
  margin-bottom: 20px;
}

/* ---------- Header ---------- */
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

/* ---------- 视频容器 ---------- */
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
  line-height: 0;
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
