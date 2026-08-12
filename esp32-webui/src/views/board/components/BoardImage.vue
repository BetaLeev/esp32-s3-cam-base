<template>
  <el-card class="board-image-card">
    <template #header>
      <span>板子实物图</span>
    </template>
    <div class="image-container">
      <el-image
        v-if="imageUrl"
        :src="imageUrl"
        :zoom-rate="1.2"
        :preview-src-list="[imageUrl]"
        fit="contain"
        class="board-image"
      >
        <template #error>
          <div class="image-placeholder">
            <el-icon :size="48"><Picture /></el-icon>
            <span>图片加载失败</span>
          </div>
        </template>
      </el-image>
      <div v-else class="image-placeholder">
        <el-icon :size="48"><Picture /></el-icon>
        <span>暂无图片</span>
      </div>
    </div>
  </el-card>
</template>

<script setup>
import { Picture } from '@element-plus/icons-vue'

defineProps({
  imageUrl: {
    type: String,
    default: ''
  }
})
</script>

<style scoped>
.board-image-card {
  margin-bottom: 20px;
}

.image-container {
  display: flex;
  justify-content: center;
  align-items: center;
  min-height: 400px;
  background: #f5f7fa;
  border-radius: 8px;
  overflow: hidden;
}

.board-image {
  width: 100%;
  max-height: 500px;
}

.image-placeholder {
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  gap: 12px;
  color: #909399;
  padding: 60px 20px;
}
</style>
