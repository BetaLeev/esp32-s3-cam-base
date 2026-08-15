<template>
  <el-tag :type="tagType" :size="size" :effect="effect">
    <el-icon v-if="showIcon" :size="iconSize" class="badge-icon">
      <component :is="iconComponent" />
    </el-icon>
    {{ displayText }}
  </el-tag>
</template>

<script setup>
import { computed } from 'vue'
import {
  Check,
  Close,
  Loading,
  Warning,
  InfoFilled,
  CircleCheck,
  CircleClose
} from '@element-plus/icons-vue'

const props = defineProps({
  // 状态值：true/false/字符串
  status: {
    type: [Boolean, String],
    default: null
  },
  // 自定义显示文本
  text: {
    type: String,
    default: ''
  },
  // 标签大小
  size: {
    type: String,
    default: 'small',
    validator: (v) => ['large', 'default', 'small'].includes(v)
  },
  // 主题效果
  effect: {
    type: String,
    default: 'light',
    validator: (v) => ['light', 'dark', 'plain'].includes(v)
  },
  // 是否显示图标
  showIcon: {
    type: Boolean,
    default: true
  },
  // 图标大小
  iconSize: {
    type: Number,
    default: 12
  }
})

// 状态到类型的映射
const statusTypeMap = {
  true: 'success',
  false: 'danger',
  success: 'success',
  error: 'danger',
  warning: 'warning',
  info: 'info',
  online: 'success',
  offline: 'danger',
  connected: 'success',
  disconnected: 'info',
  running: 'success',
  stopped: 'info',
  idle: 'info',
  active: 'success'
}

// 状态到图标的映射
const statusIconMap = {
  true: CircleCheck,
  false: CircleClose,
  success: CircleCheck,
  error: CircleClose,
  warning: Warning,
  info: InfoFilled,
  online: CircleCheck,
  offline: CircleClose,
  connected: CircleCheck,
  disconnected: CircleClose,
  running: Check,
  stopped: Close,
  idle: Loading,
  active: Check
}

// 计算标签类型
const tagType = computed(() => {
  if (typeof props.status === 'boolean') {
    return props.status ? 'success' : 'danger'
  }
  if (typeof props.status === 'string') {
    return statusTypeMap[props.status] || 'info'
  }
  return 'info'
})

// 计算显示文本
const displayText = computed(() => {
  if (props.text) return props.text

  if (typeof props.status === 'boolean') {
    return props.status ? '在线' : '离线'
  }
  if (typeof props.status === 'string') {
    // 如果是已知状态，转换为中文
    const textMap = {
      success: '成功',
      error: '失败',
      warning: '警告',
      info: '未知',
      online: '在线',
      offline: '离线',
      connected: '已连接',
      disconnected: '未连接',
      running: '运行中',
      stopped: '已停止',
      idle: '空闲',
      active: '激活'
    }
    return textMap[props.status] || props.status
  }

  return '--'
})

// 计算图标组件
const iconComponent = computed(() => {
  if (typeof props.status === 'boolean') {
    return props.status ? CircleCheck : CircleClose
  }
  if (typeof props.status === 'string') {
    return statusIconMap[props.status] || InfoFilled
  }
  return InfoFilled
})
</script>

<style scoped lang="scss">
.badge-icon {
  margin-right: 4px;
  vertical-align: middle;
}
</style>
