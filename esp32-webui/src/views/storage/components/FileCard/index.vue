<template>
  <div
    class="file-card"
    :class="{
      'file-card-selected': selected,
      'file-card-dir': file.is_dir
    }"
    @click="$emit('click', file)"
    @dblclick="$emit('dblclick', file)"
  >
    <div class="file-icon-wrapper" :class="getFileIconClass(file)">
      {{ getFileIcon(file) }}
    </div>
    <div class="file-name" :title="file.name">{{ file.name }}</div>
    <div class="file-size" v-if="!file.is_dir">{{ formatBytes(file.size) }}</div>
    <div class="file-actions" @click.stop>
      <slot name="actions" :file="file" />
    </div>
  </div>
</template>

<script setup>
defineProps({
  file: {
    type: Object,
    required: true
  },
  selected: {
    type: Boolean,
    default: false
  }
})

defineEmits(['click', 'dblclick'])

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
</style>
