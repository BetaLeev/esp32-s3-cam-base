<template>
  <div class="video-view">
    <!-- 视频显示卡片（含控制按钮插槽） -->
    <VideoDisplay
      :info="info"
      :isStreaming="isStreaming"
      :fps="fps"
      :loading="loading"
      :snapshotUrl="snapshotUrl"
      :snapshotKey="snapshotKey"
      :setImgRef="setImgRef"
      @refresh-info="refreshInfo"
      @snapshot-load="onSnapshotLoad"
      @snapshot-error="onSnapshotError"
    >
      <template #controls>
        <VideoControls
          :info="info"
          :isStreaming="isStreaming"
          :actionLoading="actionLoading"
          @start-stream="handleStartStream"
          @stop-stream="handleStopStream"
          @take-snapshot="handleTakeSnapshot"
          @download-snapshot="handleDownloadSnapshot"
        />
      </template>
    </VideoDisplay>

    <!-- 摄像头参数卡片 -->
    <VideoParams
      v-if="info.initialized"
      :params="params"
      :framesizeList="framesizeList"
      :paramsChanged="paramsChanged"
      :actionLoading="actionLoading"
      @update-param="onUpdateParam"
      @apply-params="applyParams"
    />

    <!-- AI 视觉识别卡片（预留接口） -->
    <AIVision ref="aiVisionRef" @openOverlay="showOverlay = true" />

    <!-- 摄像头信息卡片 -->
    <VideoInfo
      v-if="info.initialized"
      :info="info"
      :isStreaming="isStreaming"
    />

    <!-- 可拖拽的 AI 视觉识别覆盖面板 -->
    <AIVisionOverlay
      :visible="showOverlay"
      :isStreaming="isStreaming"
      @close="showOverlay = false"
    />
  </div>
</template>

<script setup>
import { ref, watch } from 'vue'
import VideoDisplay from './components/VideoDisplay.vue'
import VideoControls from './components/VideoControls.vue'
import VideoParams from './components/VideoParams.vue'
import VideoInfo from './components/VideoInfo.vue'
import AIVision from './components/AIVision.vue'
import AIVisionOverlay from './components/AIVisionOverlay.vue'
import { useVideo } from './composables/useVideo'

const {
  // 状态
  loading,
  actionLoading,
  isStreaming,
  fps,
  snapshotKey,
  paramsChanged,
  info,
  params,
  framesizeList,
  // URL
  snapshotUrl,
  // 数据获取
  refreshInfo,
  // 图片事件
  onSnapshotLoad,
  onSnapshotError,
  // 用户操作
  markParamsChanged,
  handleStartStream,
  handleStopStream,
  handleTakeSnapshot,
  handleDownloadSnapshot,
  applyParams,
  // 辅助
  setImgRef
} = useVideo()

// AI 视觉组件引用
const aiVisionRef = ref(null)

// AI 覆盖面板可见性
const showOverlay = ref(false)

// 参数变更处理
const onUpdateParam = ({ key, value }) => {
  params[key] = value
  markParamsChanged()
}

// 当开始视频流时启动 AI 识别
watch(isStreaming, (streaming) => {
  if (aiVisionRef.value) {
    if (streaming) {
      aiVisionRef.value.start()
    } else {
      aiVisionRef.value.stop()
    }
  }
})
</script>

<style scoped>
.video-view {
  max-width: 960px;
  margin: 0 auto;
  padding-bottom: 24px;
}
</style>
