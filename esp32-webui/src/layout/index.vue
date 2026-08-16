<template>
  <div class="app-container">
    <el-container class="layout-container">
      <AppHeader />
      <AppMain />
      <AppFooter />
    </el-container>
  </div>
</template>

<script setup>
import AppHeader from './components/AppHeader.vue'
import AppMain from './components/AppMain.vue'
import AppFooter from './components/AppFooter.vue'
</script>

<style lang="scss" scoped>
@import '@/styles/main';

.app-container {
  min-height: 100vh;
  background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
  padding: $spacing-lg;
}

.layout-container {
  max-width: $page-max-width;
  margin: 0 auto;
  height: calc(100vh - #{$spacing-lg} * 2);
  border-radius: $border-radius-large;
  overflow: hidden;
  flex-direction: column;
}
</style>
