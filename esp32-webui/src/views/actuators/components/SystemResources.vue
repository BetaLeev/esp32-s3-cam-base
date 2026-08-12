<template>
  <el-card class="system-card">
    <template #header>
      <span>系统资源</span>
    </template>
    <el-descriptions :column="2" border size="small">
      <el-descriptions-item label="DRAM">
        {{ formatBytes(status.dram_free) }} / {{ formatBytes(status.dram_total) }}
      </el-descriptions-item>
      <el-descriptions-item label="PSRAM">
        {{ formatBytes(status.psram_free) }} / {{ formatBytes(status.psram_total) }}
      </el-descriptions-item>
      <el-descriptions-item label="Flash">
        {{ formatBytes(status.flash_total) }}
      </el-descriptions-item>
      <el-descriptions-item label="SPIFFS">
        {{ formatBytes(status.spiffs_free) }} / {{ formatBytes(status.spiffs_total) }}
      </el-descriptions-item>
      <el-descriptions-item label="TF卡">
        <span :class="status.sdcard_mounted ? 'text-success' : 'text-muted'">
          {{ status.sdcard_mounted ? formatBytes(status.sdcard_free) + ' 可用' : '未挂载' }}
        </span>
      </el-descriptions-item>
      <el-descriptions-item label="运行时间">
        {{ formatUptime(status.uptime_seconds) }}
      </el-descriptions-item>
    </el-descriptions>
  </el-card>
</template>

<script setup>
const props = defineProps({
  status: {
    type: Object,
    default: () => ({})
  }
})

const formatBytes = (bytes) => {
  if (!bytes) return '--'
  bytes = Number(bytes)
  if (bytes >= 1024 * 1024 * 1024) return (bytes / (1024 * 1024 * 1024)).toFixed(2) + ' GB'
  if (bytes >= 1024 * 1024) return (bytes / (1024 * 1024)).toFixed(2) + ' MB'
  if (bytes >= 1024) return (bytes / 1024).toFixed(2) + ' KB'
  return bytes + ' B'
}

const formatUptime = (seconds) => {
  if (!seconds) return '--'
  const h = Math.floor(seconds / 3600)
  const m = Math.floor((seconds % 3600) / 60)
  const s = seconds % 60
  return `${h}小时${m}分${s}秒`
}
</script>

<style scoped>
.system-card {
  margin-bottom: 20px;
}

.text-success {
  color: #67c23a;
}

.text-muted {
  color: #909399;
}
</style>
