<template>
  <div class="app-container">
    <el-container>
      <el-header>
        <h1>{{ title }}</h1>
        <el-tag type="info" v-if="envMode === 'development'">开发模式</el-tag>
      </el-header>
      <el-main>
        <router-view />
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
            设备控制
          </el-menu-item>
          <el-menu-item index="/sensors">
            <el-icon><DataAnalysis /></el-icon>
            环境监测
          </el-menu-item>
          <el-menu-item index="/video">
            <el-icon><VideoCamera /></el-icon>
            视频监控
          </el-menu-item>
          <el-menu-item index="/files">
            <el-icon><Folder /></el-icon>
            文件管理
          </el-menu-item>
          <el-menu-item index="/wifi">
            <el-icon><Connection /></el-icon>
            网络管理
          </el-menu-item>
          <el-menu-item index="/board">
            <el-icon><Cpu /></el-icon>
            板子
          </el-menu-item>
          <el-menu-item index="/ai">
            <el-icon><ChatDotRound /></el-icon>
            AI 助手
          </el-menu-item>
        </el-menu>
        <el-text type="info" size="small" class="footer-text">
          ESP32 Web UI | API: {{ apiUrl }}
        </el-text>
      </el-footer>
    </el-container>
  </div>
</template>

<script setup>
import { computed } from 'vue'
import { useRoute } from 'vue-router'
import { Monitor, DataAnalysis, VideoCamera, Folder, Connection, Cpu, ChatDotRound } from '@element-plus/icons-vue'

const route = useRoute()
const title = import.meta.env.VITE_APP_TITLE || 'ESP32 控制面板'
const envMode = import.meta.env.MODE
const apiUrl = import.meta.env.VITE_API_BASE_URL || '相对路径 (生产)'
const currentRoute = computed(() => route.path)
</script>

<style>
* {
  margin: 0;
  padding: 0;
  box-sizing: border-box;
}

body {
  font-family: 'Helvetica Neue', Helvetica, 'PingFang SC', 'Hiragino Sans GB', Arial, sans-serif;
}

.app-container {
  min-height: 100vh;
  background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
  padding: 20px;
}

.el-container {
  max-width: 1100px;
  margin: 0 auto;
}

.el-header {
  background: rgba(255, 255, 255, 0.95);
  border-radius: 12px 12px 0 0;
  padding: 20px 30px;
  display: flex;
  align-items: center;
  justify-content: space-between;
  box-shadow: 0 2px 12px rgba(0, 0, 0, 0.1);
}

.el-header h1 {
  font-size: 24px;
  color: #303133;
  margin: 0;
}

.el-main {
  background: rgba(255, 255, 255, 0.9);
  padding: 30px;
  min-height: 500px;
}

.el-footer {
  background: rgba(255, 255, 255, 0.95);
  border-radius: 0 0 12px 12px;
  padding: 0 30px 15px;
  box-shadow: 0 2px 12px rgba(0, 0, 0, 0.1);
}

.nav-menu {
  border: none;
  background: transparent;
}

.footer-text {
  display: block;
  text-align: center;
  margin-top: 10px;
}
</style>
