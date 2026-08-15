<template>
  <div class="file-manager">
    <PageCard title="文件管理器" icon="Folder" :refreshable="true" :loading="loading" @refresh="fetchData">
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

      <div class="storage-info" v-if="storageInfo.mounted">
        <el-progress :percentage="storagePercent" :stroke-width="6" />
        <div class="storage-text">
          <span>已用 {{ formatBytes(storageInfo.used) }} / {{ formatBytes(storageInfo.total) }} ({{ storagePercent }}%)</span>
          <el-tag :type="storageInfo.mounted ? 'success' : 'danger'" size="small">
            {{ storageInfo.mounted ? '已挂载' : '未挂载' }}
          </el-tag>
        </div>
      </div>
      <el-alert v-else title="TF卡未挂载" type="warning" :closable="false" />

      <div class="path-nav">
        <el-breadcrumb separator="/">
          <el-breadcrumb-item>
            <span class="path-link" @click="navigateTo('')">根目录</span>
          </el-breadcrumb-item>
          <el-breadcrumb-item v-for="(part, index) in pathParts" :key="index">
            <span class="path-link" @click="navigateTo(pathParts.slice(0, index + 1).join('/'))">
              {{ part }}
            </span>
          </el-breadcrumb-item>
        </el-breadcrumb>
      </div>

      <div class="file-grid" v-loading="loading">
        <!-- 返回上级按钮 -->
        <div v-if="currentPath" class="file-card back-card" @click="goBack">
          <div class="file-icon-wrapper back-icon">..</div>
          <div class="file-name">返回上级</div>
        </div>

        <!-- 文件/目录卡片 -->
        <div
          v-for="file in fileList"
          :key="file.url || file.name"
          class="file-card"
          :class="{ 'file-card-selected': selectedFile?.url === file.url }"
          @click="handleItemClick(file)"
          @dblclick="handleItemClick(file)"
        >
          <div class="file-icon-wrapper" :class="getFileIconClass(file)">
            {{ getFileIcon(file) }}
          </div>
          <div class="file-name" :title="file.name">{{ file.name }}</div>
          <div class="file-size" v-if="!file.is_dir">{{ formatBytes(file.size) }}</div>
          <div class="file-actions" @click.stop>
            <el-dropdown trigger="click">
              <el-button size="small" text>
                <el-icon><MoreFilled /></el-icon>
              </el-button>
              <template #dropdown>
                <el-dropdown-menu>
                  <el-dropdown-item v-if="file.is_dir" @click="openInNewWindow(file)">
                    <el-icon><FolderOpened /></el-icon> 进入目录
                  </el-dropdown-item>
                  <el-dropdown-item v-if="!file.is_dir && isMediaFile(file)" @click="openPreview(file)">
                    <el-icon><View /></el-icon> 预览
                  </el-dropdown-item>
                  <el-dropdown-item v-if="!file.is_dir && isAudioFile(file)" @click="playOnEsp32(file)">
                    <el-icon><VideoPlay /></el-icon> ESP32播放
                  </el-dropdown-item>
                  <el-dropdown-item v-if="!file.is_dir" @click="downloadFile(file)">
                    <el-icon><Download /></el-icon> 下载
                  </el-dropdown-item>
                  <el-dropdown-item divided @click="confirmDelete(file)">
                    <el-icon><Delete /></el-icon> 删除
                  </el-dropdown-item>
                </el-dropdown-menu>
              </template>
            </el-dropdown>
          </div>
        </div>

        <!-- 空状态 -->
        <div v-if="fileList.length === 0 && !loading" class="empty-state">
          <EmptyState type="no-data" text="目录为空" />
        </div>
      </div>
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
import { ElMessage, ElMessageBox } from 'element-plus'
import { Upload, FolderAdd, Refresh, Folder, FolderOpened, View, Download, Delete, MoreFilled, VideoPlay } from '@element-plus/icons-vue'
import { PageCard, EmptyState } from '@/components/common'
import { getStorageInfo, getFileList, deleteFile, createDir, getFileUrl, uploadFile } from '@/api/esp32'
import { playAudioTest, stopAudio, setAudioVolume, playAudioFile } from '@/api/esp32'

const route = useRoute()
const router = useRouter()

const loading = ref(false)
const fileList = ref([])
const storageInfo = ref({})
const fileInputRef = ref(null)
const mkdirDialogVisible = ref(false)
const newDirName = ref('')
const selectedFile = ref(null)

const currentPath = computed(() => route.query.path || '')
const pathParts = computed(() => {
  return currentPath.value ? currentPath.value.split('/').filter(p => p) : []
})
const storagePercent = computed(() => {
  const total = Number(storageInfo.value.total) || 0
  const used = Number(storageInfo.value.used) || 0
  if (!total) return 0
  return Math.round((used / total) * 100)
})

const imageExts = ['jpg', 'jpeg', 'png', 'gif', 'bmp', 'webp', 'ico']
const audioExts = ['mp3', 'wav', 'flac', 'aac', 'ogg', 'm4a']
const videoExts = ['mp4', 'avi', 'mkv', 'mov', 'webm', 'flv', 'wmv', '3gp', 'mvi']

const getFileIcon = (file) => {
  if (file.is_dir) return '📁'
  const ext = file.name.split('.').pop()?.toLowerCase() || ''
  if (imageExts.includes(ext)) return '🖼️'
  if (audioExts.includes(ext)) return '🎵'
  if (videoExts.includes(ext)) return '🎬'
  if (ext === 'pdf') return '📄'
  if (['txt', 'log', 'md', 'json'].includes(ext)) return '📝'
  if (['zip', 'rar', '7z', 'tar', 'gz'].includes(ext)) return '📦'
  return '📄'
}

const getFileIconClass = (file) => {
  if (file.is_dir) return 'icon-folder'
  const ext = file.name.split('.').pop()?.toLowerCase() || ''
  if (imageExts.includes(ext)) return 'icon-image'
  if (audioExts.includes(ext)) return 'icon-audio'
  if (videoExts.includes(ext)) return 'icon-video'
  return 'icon-file'
}

const isMediaFile = (file) => {
  const ext = file.name.split('.').pop()?.toLowerCase() || ''
  return imageExts.includes(ext) || audioExts.includes(ext) || videoExts.includes(ext)
}

const isAudioFile = (file) => {
  if (file.is_dir) return false
  const ext = file.name.split('.').pop()?.toLowerCase() || ''
  return audioExts.includes(ext)
}

const getFileType = (filename) => {
  const ext = filename.split('.').pop()?.toLowerCase() || ''
  if (imageExts.includes(ext)) return 'image'
  if (audioExts.includes(ext)) return 'audio'
  if (videoExts.includes(ext)) return 'video'
  return 'file'
}

const formatBytes = (bytes) => {
  if (!bytes) return '--'
  bytes = Number(bytes)
  if (bytes >= 1024 * 1024 * 1024) return (bytes / (1024 * 1024 * 1024)).toFixed(2) + ' GB'
  if (bytes >= 1024 * 1024) return (bytes / (1024 * 1024)).toFixed(2) + ' MB'
  if (bytes >= 1024) return (bytes / 1024).toFixed(2) + ' KB'
  return bytes + ' B'
}

const fetchStorageInfo = async () => {
  try {
    const res = await getStorageInfo()
    const data = res.data
    storageInfo.value = data.data || data
  } catch (error) {
    console.error('加载存储信息失败:', error)
    storageInfo.value = { mounted: false }
  }
}

const fetchFiles = async () => {
  loading.value = true
  try {
    const res = await getFileList(currentPath.value)
    const data = res.data
    const fileData = data.data || data
    if (data.status === 'error') {
      ElMessage.error(data.message || '加载失败')
      fileList.value = []
      return
    }
    fileList.value = [...(fileData.files || [])].sort((a, b) => {
      if (a.is_dir !== b.is_dir) return b.is_dir - a.is_dir
      return a.name.localeCompare(b.name)
    })
  } catch (error) {
    ElMessage.error('加载文件列表失败')
    console.error(error)
  } finally {
    loading.value = false
  }
}

const fetchData = () => {
  fetchStorageInfo()
  fetchFiles()
}

watch(() => route.query.path, () => {
  fetchFiles()
})

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
    const newPath = currentPath.value ? currentPath.value + '/' + file.name : file.name
    navigateTo(newPath)
  } else {
    openPreview(file)
  }
}

const openPreview = (file) => {
  const type = getFileType(file.name)
  const filePath = currentPath.value ? currentPath.value + '/' + file.name : file.name

  if (type === 'image') {
    router.push({ path: '/files/preview', query: { path: filePath, name: file.name } })
  } else if (type === 'audio' || type === 'video') {
    router.push({ path: '/files/player', query: { path: filePath, name: file.name, type } })
  } else {
    downloadFile(file)
  }
}

const openInNewWindow = (file) => {
  const newPath = currentPath.value ? currentPath.value + '/' + file.name : file.name
  navigateTo(newPath)
}

const downloadFile = (file) => {
  const filePath = currentPath.value ? currentPath.value + '/' + file.name : file.name
  const url = getFileUrl(filePath)
  window.open(url, '_blank')
}

const triggerUpload = () => {
  fileInputRef.value?.click()
}

const handleFileSelect = async (event) => {
  const file = event.target.files[0]
  if (!file) return

  try {
    ElMessage.info('正在上传...')
    const res = await uploadFile(file, currentPath.value)

    if (res.data.status === 'success') {
      ElMessage.success('上传成功')
      fetchData()
    } else {
      ElMessage.error('上传失败: ' + (res.data.message || res.data.error))
    }
  } catch (error) {
    ElMessage.error('上传失败')
    console.error(error)
  } finally {
    event.target.value = ''
  }
}

const showMkdirDialog = () => {
  newDirName.value = ''
  mkdirDialogVisible.value = true
}

const playOnEsp32 = async (file) => {
  try {
    const filePath = currentPath.value ? currentPath.value + '/' + file.name : file.name
    ElMessage.info('正在播放: ' + file.name)
    
    // 设置超时
    const res = await Promise.race([
      playAudioFile(filePath),
      new Promise((_, reject) => 
        setTimeout(() => reject(new Error('timeout')), 10000)
      )
    ])

    if (res.data.success) {
      ElMessage.success('播放中: ' + file.name + ` (${Math.round(res.data.duration_ms / 1000)}秒)`)
    } else {
      const errorMsg = res.data.error || res.data.message || '未知错误'
      if (errorMsg.includes('格式不支持')) {
        ElMessage.warning('MP3 格式暂不支持，请使用 WAV 格式')
      } else {
        ElMessage.error('播放失败: ' + errorMsg)
      }
    }
  } catch (error) {
    if (error.message === 'timeout') {
      ElMessage.warning('播放超时，MP3 格式暂不支持')
    } else {
      ElMessage.error('播放失败')
    }
    console.error(error)
  }
}

const handleCreateDir = async () => {
  if (!newDirName.value.trim()) {
    ElMessage.warning('请输入文件夹名称')
    return
  }
  const dirPath = currentPath.value ? currentPath.value + '/' + newDirName.value : newDirName.value
  try {
    const res = await createDir(dirPath)
    if (res.data.status === 'success') {
      ElMessage.success('创建成功')
      mkdirDialogVisible.value = false
      fetchFiles()
    } else {
      ElMessage.error(res.data.message || '创建失败')
    }
  } catch (error) {
    ElMessage.error('创建失败')
  }
}

const confirmDelete = (file) => {
  const type = file.is_dir ? '文件夹' : '文件'
  ElMessageBox.confirm(`确定要删除 ${type} "${file.name}" 吗？此操作不可恢复！`, '确认删除', {
    confirmButtonText: '删除',
    cancelButtonText: '取消',
    type: 'warning'
  }).then(() => handleDelete(file)).catch(() => {})
}

const handleDelete = async (file) => {
  const itemPath = currentPath.value ? currentPath.value + '/' + file.name : file.name
  try {
    const res = await deleteFile(itemPath)
    if (res.data.status === 'success') {
      ElMessage.success('删除成功')
      fetchData()
    } else {
      ElMessage.error(res.data.message || '删除失败')
    }
  } catch (error) {
    ElMessage.error('删除失败')
  }
}

onMounted(fetchData)
</script>

<style scoped lang="scss">
@import '@/styles/variables';

.file-manager {
  max-width: 100%;
}

.header-actions {
  display: flex;
  gap: $spacing-sm;
}

.storage-info {
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

.path-nav {
  margin-bottom: $spacing-base;
  padding: $spacing-md;
  background: $bg-color;
  border-radius: $border-radius-base;
}

.path-link {
  color: $primary-color;
  cursor: pointer;
  font-size: $font-size-sm;
}

.path-link:hover {
  text-decoration: underline;
}

.file-grid {
  display: grid;
  grid-template-columns: repeat(auto-fill, minmax(120px, 1fr));
  gap: $spacing-md;
  min-height: 200px;
}

.file-card {
  display: flex;
  flex-direction: column;
  align-items: center;
  padding: $spacing-md $spacing-sm;
  border-radius: $border-radius-base;
  cursor: pointer;
  transition: all 0.2s;
  position: relative;
  background: $bg-color-white;
  border: 1px solid $border-color-lighter;
}

.file-card:hover {
  background: $bg-color;
  border-color: $primary-color;
  transform: translateY(-2px);
  box-shadow: 0 2px 12px rgba($primary-color, 0.15);
}

.file-card-selected {
  background: #ecf5ff;
  border-color: $primary-color;
}

.back-card {
  background: #fafafa;
  border-style: dashed;
}

.back-card:hover {
  background: #ecf5ff;
}

.back-icon {
  font-size: 36px;
  color: $text-secondary;
  line-height: 48px;
}

.file-icon-wrapper {
  width: 48px;
  height: 48px;
  display: flex;
  align-items: center;
  justify-content: center;
  font-size: 36px;
  margin-bottom: 6px;
}

.file-name {
  font-size: $font-size-xs;
  text-align: center;
  word-break: break-all;
  overflow: hidden;
  text-overflow: ellipsis;
  display: -webkit-box;
  -webkit-line-clamp: 2;
  -webkit-box-orient: vertical;
  max-width: 100%;
  color: $text-primary;
}

.file-size {
  font-size: 11px;
  color: $text-secondary;
  margin-top: $spacing-xs;
}

.file-actions {
  position: absolute;
  top: $spacing-xs;
  right: $spacing-xs;
  opacity: 0;
  transition: opacity 0.2s;
}

.file-card:hover .file-actions {
  opacity: 1;
}

.empty-state {
  grid-column: 1 / -1;
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  padding: $spacing-xxl;
  color: $text-secondary;
}
</style>
