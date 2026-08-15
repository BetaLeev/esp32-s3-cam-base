<template>
  <el-card class="page-card" :shadow="shadow">
    <template #header>
      <div class="card-header">
        <div class="card-header-left">
          <el-icon v-if="iconComponent" :size="18" class="card-icon">
            <component :is="iconComponent" />
          </el-icon>
          <span class="card-title">{{ title }}</span>
          <el-tag v-if="tag" :type="tagType" size="small">{{ tag }}</el-tag>
        </div>
        <div class="card-header-right">
          <slot name="header-extra" />
          <el-button
            v-if="refreshable"
            type="primary"
            size="small"
            plain
            :loading="loading"
            @click="handleRefresh"
          >
            <el-icon><Refresh /></el-icon>
            刷新
          </el-button>
        </div>
      </div>
    </template>

    <div class="card-body" :class="{ 'has-loading': loading && showLoadingOverlay }">
      <slot />
      <LoadingOverlay v-if="showLoadingOverlay" :visible="loading" />
    </div>
  </el-card>
</template>

<script setup>
import { computed } from 'vue'
import {
  Refresh, Setting, Folder, Monitor, Connection, DataAnalysis,
  VideoCamera, Document, Picture, Lightning, Sunny,
  Odometer, Rank, HotWater
} from '@element-plus/icons-vue'
import LoadingOverlay from '../loading-overlay/index.vue'

// 图标名称映射
const iconMap = {
  Refresh, Setting, Folder, Monitor, Connection, DataAnalysis,
  VideoCamera, Document, Picture, Lightning, Sunny,
  Odometer, Rank, HotWater
}

const props = defineProps({
  // 标题
  title: {
    type: String,
    required: true
  },
  // 图标（可以是字符串名称或组件对象）
  icon: {
    type: [Object, String],
    default: null
  },
  // 标签文本
  tag: {
    type: String,
    default: ''
  },
  // 标签类型
  tagType: {
    type: String,
    default: 'info'
  },
  // 是否可刷新
  refreshable: {
    type: Boolean,
    default: false
  },
  // 加载状态
  loading: {
    type: Boolean,
    default: false
  },
  // 是否显示加载遮罩
  showLoadingOverlay: {
    type: Boolean,
    default: false
  },
  // 阴影
  shadow: {
    type: String,
    default: 'hover'
  }
})

const emit = defineEmits(['refresh'])

// 解析图标（支持字符串名称或组件）
const iconComponent = computed(() => {
  if (!props.icon) return null
  if (typeof props.icon === 'object') return props.icon
  return iconMap[props.icon] || null
})

const handleRefresh = () => {
  emit('refresh')
}
</script>

<style scoped lang="scss">
@import '@/styles/variables';
@import '@/styles/mixins';

.page-card {
  @include card-style;
  margin-bottom: $card-gap;
}

.card-header {
  @include flex-between;
  min-height: 24px;
}

.card-header-left {
  @include flex-center;
  gap: $spacing-sm;
}

.card-header-right {
  @include flex-center;
  gap: $spacing-sm;
}

.card-icon {
  color: $primary-color;
  vertical-align: middle;
}

.card-title {
  font-size: $font-size-md;
  font-weight: 600;
  color: $text-primary;
}

.card-body {
  position: relative;
  min-height: 40px;
}

.has-loading {
  opacity: 0.7;
}
</style>
