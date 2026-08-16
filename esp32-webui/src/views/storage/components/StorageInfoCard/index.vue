<template>
  <div class="storage-info-card">
    <div class="storage-progress" v-if="storageInfo.mounted">
      <el-progress :percentage="storagePercent" :stroke-width="6" />
      <div class="storage-text">
        <span>已用 {{ formatBytes(storageInfo.used) }} / {{ formatBytes(storageInfo.total) }} ({{ storagePercent }}%)</span>
        <el-tag :type="storageInfo.mounted ? 'success' : 'danger'" size="small">
          {{ storageInfo.mounted ? '已挂载' : '未挂载' }}
        </el-tag>
      </div>
    </div>
    <el-alert v-else title="TF卡未挂载" type="warning" :closable="false" />
  </div>
</template>

<script setup>
import { computed } from 'vue'

const props = defineProps({
  storageInfo: {
    type: Object,
    default: () => ({ mounted: false, total: 0, used: 0 })
  }
})

const storagePercent = computed(() => {
  const total = Number(props.storageInfo.total) || 0
  const used = Number(props.storageInfo.used) || 0
  if (!total) return 0
  return Math.round((used / total) * 100)
})

const formatBytes = (bytes) => {
  if (!bytes) return '--'
  bytes = Number(bytes)
  if (bytes >= 1024 * 1024 * 1024) return (bytes / (1024 * 1024 * 1024)).toFixed(2) + ' GB'
  if (bytes >= 1024 * 1024) return (bytes / (1024 * 1024)).toFixed(2) + ' MB'
  if (bytes >= 1024) return (bytes / 1024).toFixed(2) + ' KB'
  return bytes + ' B'
}
</script>

<style scoped lang="scss">
@import '@/styles/variables';

.storage-info-card {
  margin-bottom: $spacing-base;
}

.storage-text {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin-top: $spacing-sm;
  font-size: $font-size-xs;
  color: $text-regular;
}
</style>
