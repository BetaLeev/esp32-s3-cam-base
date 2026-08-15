<template>
  <transition name="fade">
    <div v-if="visible" class="loading-overlay">
      <div class="loading-spinner">
        <el-icon class="is-loading" :size="size">
          <Loading />
        </el-icon>
        <div v-if="text" class="loading-text">{{ text }}</div>
      </div>
    </div>
  </transition>
</template>

<script setup>
import { Loading } from '@element-plus/icons-vue'

defineProps({
  visible: {
    type: Boolean,
    default: false
  },
  size: {
    type: Number,
    default: 32
  },
  text: {
    type: String,
    default: ''
  }
})
</script>

<style scoped lang="scss">
@import '@/styles/variables';

.loading-overlay {
  position: absolute;
  top: 0;
  left: 0;
  right: 0;
  bottom: 0;
  display: flex;
  align-items: center;
  justify-content: center;
  background: rgba(255, 255, 255, 0.8);
  backdrop-filter: blur(2px);
  z-index: 100;
  border-radius: inherit;
}

.loading-spinner {
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: $spacing-sm;
}

.loading-text {
  font-size: $font-size-sm;
  color: $text-secondary;
}

.fade-enter-active,
.fade-leave-active {
  transition: opacity 0.2s ease;
}

.fade-enter-from,
.fade-leave-to {
  opacity: 0;
}
</style>
