<template>
  <div class="preview-view">
    <el-card>
      <template #header>
        <div class="card-header">
          <span>{{ fileName }}</span>
          <el-button @click="goBack">
            <el-icon><Back /></el-icon> 返回
          </el-button>
        </div>
      </template>

      <div class="preview-container">
        <el-image
          v-if="imageUrl"
          :src="imageUrl"
          :alt="fileName"
          fit="contain"
          @load="onLoad"
          @error="onError"
        />
        <div v-if="loading" class="loading">
          <el-icon class="is-loading"><Loading /></el-icon>
          <span>加载中...</span>
        </div>
        <div v-if="error" class="error">
          <el-icon><WarningFilled /></el-icon>
          <span>{{ error }}</span>
        </div>
      </div>
    </el-card>
  </div>
</template>

<script setup>
import { ref, computed, onMounted } from 'vue'
import { useRouter, useRoute } from 'vue-router'
import { Back, Loading, WarningFilled } from '@element-plus/icons-vue'
import { ElImage } from 'element-plus'

const router = useRouter()
const route = useRoute()

const loading = ref(true)
const error = ref('')

const fileName = computed(() => route.query.name || '预览')

const imageUrl = computed(() => {
  const path = route.query.path
  if (!path) return ''
  return `/fs/files?path=${encodeURIComponent(path)}`
})

const goBack = () => {
  router.back()
}

const onLoad = () => {
  loading.value = false
  error.value = ''
}

const onError = () => {
  loading.value = false
  error.value = '图片加载失败'
}

onMounted(() => {
  if (!route.query.path) {
    error.value = '没有文件路径'
    loading.value = false
  }
})
</script>

<style scoped>
.preview-view {
  max-width: 1000px;
  margin: 0 auto;
}

.card-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
}

.preview-container {
  display: flex;
  justify-content: center;
  align-items: center;
  min-height: 400px;
  background: #f5f7fa;
  border-radius: 8px;
  overflow: hidden;
}

.preview-container :deep(.el-image) {
  max-width: 100%;
  max-height: 70vh;
}

.loading, .error {
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 10px;
  color: #909399;
}

.error {
  color: #f56c6c;
}
</style>
