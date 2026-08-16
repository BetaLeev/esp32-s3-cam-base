<template>
  <el-footer class="app-footer">
    <AppMenu />
    <div class="footer-text">
      <span>ESP32 Web UI</span>
      <span class="separator">|</span>
      <span>API: {{ apiUrl }}</span>
    </div>
  </el-footer>
</template>

<script setup>
import AppMenu from './AppMenu.vue'

const apiUrl = import.meta.env.VITE_API_BASE_URL || '相对路径 (生产)'
</script>

<style lang="scss" scoped>
@import '@/styles/main';

.app-footer {
  background: rgba(255, 255, 255, 0.95);
  border-radius: 0 0 $border-radius-large $border-radius-large;
  padding: 0 $page-padding $spacing-base;
  box-shadow: $shadow-base;
}

.footer-text {
  text-align: center;
  color: $text-secondary;
  font-size: $font-size-sm;
  margin-top: $spacing-sm;

  .separator {
    margin: 0 $spacing-sm;
    color: $border-color;
  }
}
</style>
