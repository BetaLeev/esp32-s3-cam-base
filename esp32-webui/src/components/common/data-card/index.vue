<template>
  <div class="data-card" :class="{ 'has-icon': icon }">
    <div v-if="icon" class="data-icon">
      <el-icon :size="iconSize">
        <component :is="icon" />
      </el-icon>
    </div>
    <div class="data-content">
      <div class="data-value">
        {{ displayValue }}
        <span v-if="unit" class="data-unit">{{ unit }}</span>
      </div>
      <div class="data-label">{{ label }}</div>
    </div>
  </div>
</template>

<script setup>
import { computed } from 'vue'

const props = defineProps({
  // 标签
  label: {
    type: String,
    required: true
  },
  // 数值
  value: {
    type: [Number, String],
    default: null
  },
  // 单位
  unit: {
    type: String,
    default: ''
  },
  // 图标组件
  icon: {
    type: [Object, String],
    default: null
  },
  // 图标大小
  iconSize: {
    type: Number,
    default: 24
  },
  // 小数位数
  decimals: {
    type: Number,
    default: 2
  },
  // 格式化函数
  formatter: {
    type: Function,
    default: null
  }
})

// 计算显示值
const displayValue = computed(() => {
  if (props.value === null || props.value === undefined) {
    return '--'
  }

  if (props.formatter) {
    return props.formatter(props.value)
  }

  const num = Number(props.value)
  if (isNaN(num)) {
    return props.value
  }

  // 如果是整数或没有小数部分，返回整数
  if (Number.isInteger(num) || props.decimals === 0) {
    return num.toString()
  }

  return num.toFixed(props.decimals)
})
</script>

<style scoped lang="scss">
@import '@/styles/variables';
@import '@/styles/mixins';

.data-card {
  @include flex-center;
  flex-direction: column;
  padding: $spacing-lg;
  background: $bg-color;
  border-radius: $border-radius-base;
  text-align: center;
  transition: background-color 0.2s ease;

  &:hover {
    background: #eef1f5;
  }

  &.has-icon {
    flex-direction: row;
    text-align: left;
    gap: $spacing-base;
  }
}

.data-icon {
  @include flex-center;
  width: 48px;
  height: 48px;
  background: $bg-color-white;
  border-radius: $border-radius-base;
  color: $primary-color;
  flex-shrink: 0;
}

.data-content {
  flex: 1;
  min-width: 0;
}

.data-value {
  font-size: $font-size-xxl;
  font-weight: bold;
  color: $text-primary;
  line-height: 1.2;
}

.data-unit {
  font-size: $font-size-sm;
  color: $text-secondary;
  margin-left: 2px;
}

.data-label {
  margin-top: $spacing-xs;
  font-size: $font-size-sm;
  color: $text-secondary;
}

.has-icon {
  .data-value {
    font-size: $font-size-xl;
  }

  .data-label {
    margin-top: 0;
  }
}
</style>
