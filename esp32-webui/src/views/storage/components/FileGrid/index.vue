<template>
  <div class="file-grid" v-loading="loading">
    <!-- 返回上级按钮 -->
    <div v-if="showBack" class="file-card back-card" @click="$emit('back')">
      <div class="file-icon-wrapper back-icon">..</div>
      <div class="file-name">返回上级</div>
    </div>

    <!-- 文件/目录卡片 -->
    <FileCard
      v-for="file in files"
      :key="file.url || file.name"
      :file="file"
      :selected="selectedFile?.url === file.url"
      @click="$emit('item-click', file)"
      @dblclick="$emit('item-dblclick', file)"
    >
      <template #actions="{ file: f }">
        <el-dropdown trigger="click">
          <el-button size="small" text>
            <el-icon><MoreFilled /></el-icon>
          </el-button>
          <template #dropdown>
            <el-dropdown-menu>
              <el-dropdown-item v-if="f.is_dir" @click="$emit('open-dir', f)">
                <el-icon><FolderOpened /></el-icon> 进入目录
              </el-dropdown-item>
              <el-dropdown-item v-if="!f.is_dir && isMediaFile(f)" @click="$emit('preview', f)">
                <el-icon><View /></el-icon> 预览
              </el-dropdown-item>
              <el-dropdown-item v-if="!f.is_dir && isAudioFile(f)" @click="$emit('play', f)">
                <el-icon><VideoPlay /></el-icon> ESP32播放
              </el-dropdown-item>
              <el-dropdown-item v-if="!f.is_dir" @click="$emit('download', f)">
                <el-icon><Download /></el-icon> 下载
              </el-dropdown-item>
              <el-dropdown-item divided @click="$emit('delete', f)">
                <el-icon><Delete /></el-icon> 删除
              </el-dropdown-item>
            </el-dropdown-menu>
          </template>
        </el-dropdown>
      </template>
    </FileCard>

    <!-- 空状态 -->
    <div v-if="files.length === 0 && !loading" class="empty-state">
      <EmptyState type="no-data" text="目录为空" />
    </div>
  </div>
</template>

<script setup>
import { FolderOpened, View, Download, Delete, MoreFilled, VideoPlay } from '@element-plus/icons-vue'
import { EmptyState } from '@/components/common'
import FileCard from '../FileCard/index.vue'

defineProps({
  files: {
    type: Array,
    default: () => []
  },
  selectedFile: {
    type: Object,
    default: null
  },
  loading: {
    type: Boolean,
    default: false
  },
  showBack: {
    type: Boolean,
    default: false
  }
})

defineEmits([
  'back',
  'item-click',
  'item-dblclick',
  'open-dir',
  'preview',
  'play',
  'download',
  'delete'
])

const imageExts = ['jpg', 'jpeg', 'png', 'gif', 'bmp', 'webp', 'ico']
const audioExts = ['mp3', 'wav', 'flac', 'aac', 'ogg', 'm4a']
const videoExts = ['mp4', 'avi', 'mkv', 'mov', 'webm', 'flv', 'wmv', '3gp', 'mvi']

const isMediaFile = (file) => {
  const ext = file.name.split('.').pop()?.toLowerCase() || ''
  return imageExts.includes(ext) || audioExts.includes(ext) || videoExts.includes(ext)
}

const isAudioFile = (file) => {
  if (file.is_dir) return false
  const ext = file.name.split('.').pop()?.toLowerCase() || ''
  return audioExts.includes(ext)
}
</script>

<style scoped lang="scss">
@import '@/styles/variables';

.file-grid {
  display: grid;
  grid-template-columns: repeat(auto-fill, minmax(120px, 1fr));
  gap: $spacing-md;
  min-height: 200px;
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
