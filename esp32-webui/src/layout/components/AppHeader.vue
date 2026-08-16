<template>
  <el-header class="app-header">
    <div class="header-content">
      <div class="header-left">
        <div class="logo">
          <el-icon class="logo-icon"><Cpu /></el-icon>
          <h1>{{ title }}</h1>
        </div>
      </div>
      <div class="header-right">
        <el-tag type="info" v-if="envMode === 'development'" size="small">开发模式</el-tag>
      </div>
    </div>
  </el-header>
</template>

<script setup>
import { Cpu } from '@element-plus/icons-vue'

const title = import.meta.env.VITE_APP_TITLE || 'ESP32 控制面板'
const envMode = import.meta.env.MODE
</script>

<style lang="scss" scoped>
@import '@/styles/main';

.app-header {
  background: rgba(255, 255, 255, 0.95);
  border-radius: $border-radius-large $border-radius-large 0 0;
  padding: 0 $page-padding;
  box-shadow: $shadow-base;
}

.header-content {
  @include flex-between;
  height: $header-height;
}

.header-left {
  @include flex-center;
}

.logo {
  @include flex-center;
  gap: $spacing-md;

  .logo-icon {
    font-size: 32px;
    color: $primary-color;
  }

  h1 {
    font-size: $font-size-xxl;
    color: $text-primary;
    margin: 0;
    font-weight: 600;
  }
}

.header-right {
  @include flex-center;
}
</style>
