<template>
  <div class="app-container">
    <el-container>
      <el-header>
        <div class="header-content">
          <div class="header-left">
            <h1>{{ title }}</h1>
          </div>
          <div class="header-right">
            <el-tag type="info" v-if="envMode === 'development'" size="small">开发模式</el-tag>
          </div>
        </div>
      </el-header>

      <el-main class="main-container">
        <div class="page-container">
          <router-view />
        </div>
      </el-main>

      <el-footer>
        <el-menu
          mode="horizontal"
          :default-active="currentRoute"
          router
          class="nav-menu"
        >
          <el-menu-item index="/actuators">
            <el-icon><Monitor /></el-icon>
            <span>设备控制</span>
          </el-menu-item>
          <el-menu-item index="/sensors">
            <el-icon><DataAnalysis /></el-icon>
            <span>环境监测</span>
          </el-menu-item>
          <el-menu-item index="/video">
            <el-icon><VideoCamera /></el-icon>
            <span>视频监控</span>
          </el-menu-item>
          <el-menu-item index="/files">
            <el-icon><Folder /></el-icon>
            <span>文件管理</span>
          </el-menu-item>
          <el-menu-item index="/wifi">
            <el-icon><Connection /></el-icon>
            <span>网络管理</span>
          </el-menu-item>
          <el-menu-item index="/board">
            <el-icon><Cpu /></el-icon>
            <span>板子</span>
          </el-menu-item>
          <el-menu-item index="/ai">
            <el-icon><ChatDotRound /></el-icon>
            <span>AI 助手</span>
          </el-menu-item>
        </el-menu>
        <div class="footer-text">
          ESP32 Web UI | API: {{ apiUrl }}
        </div>
      </el-footer>
    </el-container>
  </div>
</template>

<script setup>
import { computed } from 'vue'
import { useRoute } from 'vue-router'
import {
  Monitor,
  DataAnalysis,
  VideoCamera,
  Folder,
  Connection,
  Cpu,
  ChatDotRound
} from '@element-plus/icons-vue'

const route = useRoute()
const title = import.meta.env.VITE_APP_TITLE || 'ESP32 控制面板'
const envMode = import.meta.env.MODE
const apiUrl = import.meta.env.VITE_API_BASE_URL || '相对路径 (生产)'
const currentRoute = computed(() => route.path)
</script>

<style lang="scss">
@import '@/styles/main';

.app-container {
  min-height: 100vh;
  background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
  padding: $spacing-lg;
}

.el-container {
  max-width: $page-max-width;
  margin: 0 auto;
}

.el-header {
  background: rgba(255, 255, 255, 0.95);
  border-radius: $border-radius-large $border-radius-large 0 0;
  padding: 0 $page-padding;
  box-shadow: $shadow-base;
}

.header-content {
  @include flex-between;
  height: $header-height;
}

.header-left h1 {
  font-size: $font-size-xxl;
  color: $text-primary;
  margin: 0;
}

.header-right {
  @include flex-center;
}

.el-main {
  background: rgba(255, 255, 255, 0.9);
  padding: 0;
}

.main-container {
  padding: $spacing-lg 0;
}

.page-container {
  padding: 0 $page-padding;
  max-width: 100%;
}

.el-footer {
  background: rgba(255, 255, 255, 0.95);
  border-radius: 0 0 $border-radius-large $border-radius-large;
  padding: 0 $page-padding $spacing-base;
  box-shadow: $shadow-base;
}

.nav-menu {
  border: none;
  background: transparent;
}

.footer-text {
  text-align: center;
  color: $text-secondary;
  font-size: $font-size-sm;
  margin-top: $spacing-sm;
}
</style>
