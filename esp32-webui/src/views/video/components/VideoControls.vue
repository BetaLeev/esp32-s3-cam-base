<template>
  <div class="controls">
    <el-button
      type="primary"
      :icon="VideoPlay"
      @click="emit('startStream')"
      :disabled="!info.initialized || isStreaming"
      :loading="actionLoading === 'start'"
    >
      开启实时流
    </el-button>
    <el-button
      type="danger"
      :icon="VideoPause"
      @click="emit('stopStream')"
      :disabled="!isStreaming"
      :loading="actionLoading === 'stop'"
    >
      停止实时流
    </el-button>
    <el-button
      :icon="Camera"
      @click="emit('takeSnapshot')"
      :disabled="!info.initialized"
    >
      抓拍快照
    </el-button>
    <el-button
      :icon="Download"
      @click="emit('downloadSnapshot')"
      :disabled="!info.initialized"
    >
      下载图片
    </el-button>
  </div>
</template>

<script setup>
import { VideoPlay, VideoPause, Camera, Download } from '@element-plus/icons-vue'

defineProps({
  // 摄像头信息
  info: { type: Object, required: true },
  // 是否处于实时流模式
  isStreaming: { type: Boolean, default: false },
  // 当前操作加载态: 'start' | 'stop' | 'config' | ''
  actionLoading: { type: String, default: '' }
})

const emit = defineEmits(['startStream', 'stopStream', 'takeSnapshot', 'downloadSnapshot'])
</script>

<style scoped>
.controls {
  display: flex;
  flex-wrap: wrap;
  gap: 10px;
  margin-top: 16px;
}

.controls .el-button {
  flex: 1 1 140px;
}
</style>
