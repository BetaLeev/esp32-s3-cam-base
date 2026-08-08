<template>
  <div class="video-view">
    <!-- 视频显示卡片（含控制按钮插槽） -->
    <VideoDisplay
      :info="info"
      :isStreaming="isStreaming"
      :fps="fps"
      :loading="loading"
      :streamUrl="streamUrl"
      :snapshotUrl="snapshotUrl"
      :snapshotKey="snapshotKey"
      @refresh-info="refreshInfo"
      @snapshot-load="onSnapshotLoad"
      @snapshot-error="onSnapshotError"
      @stream-load="onStreamLoad"
      @stream-error="onStreamError"
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

    <!-- 摄像头信息卡片 -->
    <VideoInfo
      v-if="info.initialized"
      :info="info"
      :isStreaming="isStreaming"
    />
  </div>
</template>

<script setup>
import VideoDisplay from './components/VideoDisplay.vue'
import VideoControls from './components/VideoControls.vue'
import VideoParams from './components/VideoParams.vue'
import VideoInfo from './components/VideoInfo.vue'
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
  streamUrl,
  snapshotUrl,
  // 数据获取
  refreshInfo,
  // 图片事件
  onSnapshotLoad,
  onSnapshotError,
  onStreamLoad,
  onStreamError,
  // 用户操作
  markParamsChanged,
  handleStartStream,
  handleStopStream,
  handleTakeSnapshot,
  handleDownloadSnapshot,
  applyParams
} = useVideo()

// 参数变更处理：更新对应字段并标记已修改
const onUpdateParam = ({ key, value }) => {
  params[key] = value
  markParamsChanged()
}
</script>

<style scoped>
.video-view {
  max-width: 960px;
  margin: 0 auto;
  padding-bottom: 24px;
}
</style>
