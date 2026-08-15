<template>
  <div class="empty-state">
    <div class="empty-icon">
      <el-icon :size="iconSize">
        <component :is="icon" />
      </el-icon>
    </div>
    <div class="empty-text">{{ text }}</div>
    <div v-if="$slots.action" class="empty-action">
      <slot name="action" />
    </div>
  </div>
</template>

<script setup>
import { computed } from 'vue'
import { FolderOpened, Document, Warning, CircleClose } from '@element-plus/icons-vue'

const props = defineProps({
  // 显示文本
  text: {
    type: String,
    default: '暂无数据'
  },
  // 图标
  icon: {
    type: [Object, String],
    default: FolderOpened
  },
  // 图标大小
  iconSize: {
    type: Number,
    default: 48
  },
  // 类型：default | no-data | error | no-result
  type: {
    type: String,
    default: 'default',
    validator: (v) => ['default', 'no-data', 'error', 'no-result'].includes(v)
  }
})

const icon = computed(() => {
  if (props.icon !== FolderOpened) return props.icon

  const iconMap = {
    'no-data': Document,
    'error': CircleClose,
    'no-result': Warning
  }
  return iconMap[props.type] || FolderOpened
})
</script>

<style scoped lang="scss">
@import '@/styles/variables';

.empty-state {
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  padding: $spacing-xxl;
  color: $text-secondary;
}

.empty-icon {
  margin-bottom: $spacing-base;
  color: $text-placeholder;
}

.empty-text {
  font-size: $font-size-base;
  text-align: center;
}

.empty-action {
  margin-top: $spacing-base;
}
</style>
