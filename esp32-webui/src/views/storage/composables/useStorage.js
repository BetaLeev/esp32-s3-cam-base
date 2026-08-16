import { ref, computed } from 'vue'
import { ElMessage, ElMessageBox } from 'element-plus'
import {
  getStorageInfo,
  getFileList,
  deleteFile,
  createDir,
  getFileUrl,
  uploadFile,
  playAudioFile
} from '@/api/esp32'

export function useStorage() {
  const loading = ref(false)
  const fileList = ref([])
  const storageInfo = ref({})
  const selectedFile = ref(null)

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

  const fetchFiles = async (currentPath) => {
    loading.value = true
    try {
      const res = await getFileList(currentPath)
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

  const fetchData = async (currentPath) => {
    await Promise.all([fetchStorageInfo(), fetchFiles(currentPath)])
  }

  const handleDelete = async (file, currentPath) => {
    const type = file.is_dir ? '文件夹' : '文件'
    try {
      await ElMessageBox.confirm(
        `确定要删除 ${type} "${file.name}" 吗？此操作不可恢复！`,
        '确认删除',
        {
          confirmButtonText: '删除',
          cancelButtonText: '取消',
          type: 'warning'
        }
      )
    } catch {
      return
    }

    const itemPath = currentPath ? currentPath + '/' + file.name : file.name
    try {
      const res = await deleteFile(itemPath)
      if (res.data.status === 'success') {
        ElMessage.success('删除成功')
        fetchData(currentPath)
      } else {
        ElMessage.error(res.data.message || '删除失败')
      }
    } catch (error) {
      ElMessage.error('删除失败')
    }
  }

  const handleCreateDir = async (dirName, currentPath) => {
    if (!dirName?.trim()) {
      ElMessage.warning('请输入文件夹名称')
      return false
    }
    const dirPath = currentPath ? currentPath + '/' + dirName : dirName
    try {
      const res = await createDir(dirPath)
      if (res.data.status === 'success') {
        ElMessage.success('创建成功')
        fetchFiles(currentPath)
        return true
      } else {
        ElMessage.error(res.data.message || '创建失败')
        return false
      }
    } catch (error) {
      ElMessage.error('创建失败')
      return false
    }
  }

  const handleUpload = async (file, currentPath) => {
    if (!file) return false
    try {
      ElMessage.info('正在上传...')
      const res = await uploadFile(file, currentPath)
      if (res.data.status === 'success') {
        ElMessage.success('上传成功')
        fetchData(currentPath)
        return true
      } else {
        ElMessage.error('上传失败: ' + (res.data.message || res.data.error))
        return false
      }
    } catch (error) {
      ElMessage.error('上传失败')
      console.error(error)
      return false
    }
  }

  const handlePlay = async (file, currentPath) => {
    const filePath = currentPath ? currentPath + '/' + file.name : file.name
    try {
      ElMessage.info('正在播放: ' + file.name)

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
    }
  }

  const downloadFile = (file, currentPath) => {
    const filePath = currentPath ? currentPath + '/' + file.name : file.name
    const url = getFileUrl(filePath)
    window.open(url, '_blank')
  }

  const getFileType = (filename) => {
    const imageExts = ['jpg', 'jpeg', 'png', 'gif', 'bmp', 'webp', 'ico']
    const audioExts = ['mp3', 'wav', 'flac', 'aac', 'ogg', 'm4a']
    const videoExts = ['mp4', 'avi', 'mkv', 'mov', 'webm', 'flv', 'wmv', '3gp', 'mvi']
    const ext = filename.split('.').pop()?.toLowerCase() || ''
    if (imageExts.includes(ext)) return 'image'
    if (audioExts.includes(ext)) return 'audio'
    if (videoExts.includes(ext)) return 'video'
    return 'file'
  }

  const getFilePath = (fileName, currentPath) => {
    return currentPath ? currentPath + '/' + fileName : fileName
  }

  return {
    loading,
    fileList,
    storageInfo,
    selectedFile,
    fetchData,
    fetchStorageInfo,
    fetchFiles,
    handleDelete,
    handleCreateDir,
    handleUpload,
    handlePlay,
    downloadFile,
    getFileType,
    getFilePath
  }
}
