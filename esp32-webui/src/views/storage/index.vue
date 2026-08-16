<template>
  <div class="storage-manager">
    <PageCard
      title="文件管理器"
      icon="Folder"
      :refreshable="true"
      :loading="loading"
      @refresh="handleRefresh"
    >
      <template #header-extra>
        <div class="header-actions">
          <el-button type="primary" size="small" @click="triggerUpload">
            <el-icon><Upload /></el-icon> 上传
          </el-button>
          <el-button size="small" @click="showMkdirDialog">
            <el-icon><FolderAdd /></el-icon> 新建
          </el-button>
        </div>
      </template>

      <StorageInfoCard :storage-info="storageInfo" />

      <PathNavigator
        :current-path="currentPath"
        @navigate="navigateTo"
      />

      <FileGrid
        :files="fileList"
        :selected-file="selectedFile"
        :loading="loading"
        :show-back="!!currentPath"
        @back="goBack"
        @item-click="handleItemClick"
        @item-dblclick="handleItemClick"
        @open-dir="handleOpenDir"
        @preview="handlePreview"
        @play="handlePlay"
        @download="handleDownload"
        @delete="handleDelete"
      />
    </PageCard>

    <input type="file" ref="fileInputRef" style="display: none" @change="handleFileSelect" />

    <el-dialog v-model="mkdirDialogVisible" title="新建文件夹" width="300px">
      <el-input v-model="newDirName" placeholder="文件夹名称" @keyup.enter="handleCreateDir" />
      <template #footer>
        <el-button @click="mkdirDialogVisible = false">取消</el-button>
        <el-button type="primary" @click="handleCreateDir">创建</el-button>
      </template>
    </el-dialog>
  </div>
</template>

<script setup>
import { ref, computed, onMounted, watch } from 'vue'
import { useRoute, useRouter } from 'vue-router'
import { Upload, FolderAdd } from '@element-plus/icons-vue'
import { PageCard } from '@/components/common'
import { useStorage } from './composables/useStorage'
import StorageInfoCard from './components/StorageInfoCard/index.vue'
import PathNavigator from './components/PathNavigator/index.vue'
import FileGrid from './components/FileGrid/index.vue'

const route = useRoute()
const router = useRouter()

const {
  loading,
  fileList,
  storageInfo,
  selectedFile,
  fetchData,
  handleDelete: storageDelete,
  handleCreateDir: storageCreateDir,
  handleUpload: storageUpload,
  handlePlay: storagePlay,
  downloadFile,
  getFileType,
  getFilePath
} = useStorage()

const fileInputRef = ref(null)
const mkdirDialogVisible = ref(false)
const newDirName = ref('')

const currentPath = computed(() => route.query.path || '')

const handleRefresh = () => {
  fetchData(currentPath.value)
}

const navigateTo = (path) => {
  router.push({ path: '/files', query: path ? { path } : {} })
}

const goBack = () => {
  const parts = currentPath.value.split('/').filter(p => p)
  parts.pop()
  navigateTo(parts.join('/'))
}

const handleItemClick = (file) => {
  selectedFile.value = file
  if (file.is_dir) {
    navigateTo(getFilePath(file.name, currentPath.value))
  } else {
    handlePreview(file)
  }
}

const handleOpenDir = (file) => {
  navigateTo(getFilePath(file.name, currentPath.value))
}

const handlePreview = (file) => {
  const type = getFileType(file.name)
  const filePath = getFilePath(file.name, currentPath.value)

  if (type === 'image') {
    router.push({ path: '/files/preview', query: { path: filePath, name: file.name } })
  } else if (type === 'audio' || type === 'video') {
    router.push({ path: '/files/player', query: { path: filePath, name: file.name, type } })
  } else {
    handleDownload(file)
  }
}

const handlePlay = (file) => {
  storagePlay(file, currentPath.value)
}

const handleDownload = (file) => {
  downloadFile(file, currentPath.value)
}

const triggerUpload = () => {
  fileInputRef.value?.click()
}

const handleFileSelect = async (event) => {
  const file = event.target.files[0]
  if (!file) return
  await storageUpload(file, currentPath.value)
  event.target.value = ''
}

const showMkdirDialog = () => {
  newDirName.value = ''
  mkdirDialogVisible.value = true
}

const handleCreateDir = async () => {
  const success = await storageCreateDir(newDirName.value, currentPath.value)
  if (success) {
    mkdirDialogVisible.value = false
  }
}

const handleDelete = (file) => {
  storageDelete(file, currentPath.value)
}

watch(() => route.query.path, () => {
  fetchData(currentPath.value)
})

onMounted(() => {
  fetchData(currentPath.value)
})
</script>

<style scoped lang="scss">
@import '@/styles/variables';

.storage-manager {
  max-width: 100%;
}

.header-actions {
  display: flex;
  gap: $spacing-sm;
}
</style>
