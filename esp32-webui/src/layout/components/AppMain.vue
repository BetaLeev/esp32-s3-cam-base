<template>
  <el-main class="app-main">
    <div class="main-container">
      <div class="page-container">
        <router-view />
      </div>
    </div>
  </el-main>
</template>

<style lang="scss" scoped>
@import '@/styles/main';

.app-main {
  background: rgba(255, 255, 255, 0.9);
  padding: 0;
  flex: 1;
  min-height: 0;
}

.main-container {
  padding: $spacing-lg 0;
}

.page-container {
  padding: 0 $page-padding;
  max-width: 100%;
}
</style>
