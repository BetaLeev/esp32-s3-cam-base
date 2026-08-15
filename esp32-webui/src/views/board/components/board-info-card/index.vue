<template>
  <el-card class="board-info-card">
    <template #header>
      <div class="card-header">
        <div class="card-header-left">
          <el-icon :size="18" class="card-icon"><Cpu /></el-icon>
          <span class="card-title">板子信息</span>
        </div>
      </div>
    </template>

    <el-descriptions :column="2" border size="small">
      <el-descriptions-item label="芯片型号">
        {{ boardInfo.chip_model || '—' }}
      </el-descriptions-item>
      <el-descriptions-item label="固件版本">
        {{ boardInfo.firmware_version || '—' }}
      </el-descriptions-item>
      <el-descriptions-item label="板子名称">
        {{ boardInfo.board_name || '—' }}
      </el-descriptions-item>
      <el-descriptions-item label="运行时间">
        {{ boardInfo.uptime || '—' }}
      </el-descriptions-item>
      <el-descriptions-item label="编译时间">
        {{ boardInfo.build_time || '—' }}
      </el-descriptions-item>
      <el-descriptions-item label="可用内存">
        {{ boardInfo.free_heap || '—' }}
      </el-descriptions-item>
    </el-descriptions>
  </el-card>
</template>

<script setup>
import { Cpu } from '@element-plus/icons-vue'

defineProps({
  boardInfo: {
    type: Object,
    default: () => ({})
  }
})
</script>

<style scoped lang="scss">
@import '@/styles/variables';
@import '@/styles/mixins';

.board-info-card {
  @include card-style;
  margin-bottom: $card-gap;
}

.card-header {
  @include flex-between;
}

.card-header-left {
  @include flex-center;
  gap: $spacing-sm;
}

.card-icon {
  color: $primary-color;
}

.card-title {
  font-size: $font-size-md;
  font-weight: 600;
  color: $text-primary;
}
</style>
