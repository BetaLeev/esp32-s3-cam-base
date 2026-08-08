<template>
  <el-card class="info-card">
    <template #header>
      <span>摄像头信息</span>
    </template>
    <el-descriptions :column="2" border size="default">
      <el-descriptions-item label="传感器型号">
        <el-tag type="primary">{{ info.sensor || '--' }}</el-tag>
      </el-descriptions-item>
      <el-descriptions-item label="分辨率">
        {{ info.resolution?.width || '--' }} × {{ info.resolution?.height || '--' }}
      </el-descriptions-item>
      <el-descriptions-item label="JPEG 质量">
        {{ info.params?.quality ?? '--' }} / 63
      </el-descriptions-item>
      <el-descriptions-item label="流状态">
        <el-tag :type="isStreaming ? 'success' : 'info'" size="small">
          {{ isStreaming ? '运行中' : '已停止' }}
        </el-tag>
      </el-descriptions-item>
      <el-descriptions-item label="增益 (AGC)">
        {{ info.params?.gain ?? '--' }}
      </el-descriptions-item>
      <el-descriptions-item label="曝光 (AEC)">
        {{ info.params?.exposure ?? '--' }}
      </el-descriptions-item>
    </el-descriptions>
  </el-card>
</template>

<script setup>
defineProps({
  // 摄像头信息
  info: { type: Object, required: true },
  // 是否处于实时流模式
  isStreaming: { type: Boolean, default: false }
})
</script>

<style scoped>
.info-card {
  margin-bottom: 20px;
}
</style>
