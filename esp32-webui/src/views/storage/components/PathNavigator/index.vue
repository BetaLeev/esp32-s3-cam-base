<template>
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
</template>

<script setup>
import { computed } from 'vue'

const props = defineProps({
  currentPath: {
    type: String,
    default: ''
  }
})

const emit = defineEmits(['navigate'])

const pathParts = computed(() => {
  return props.currentPath ? props.currentPath.split('/').filter(p => p) : []
})

const navigateTo = (path) => {
  emit('navigate', path)
}
</script>

<style scoped lang="scss">
@import '@/styles/variables';

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
</style>
