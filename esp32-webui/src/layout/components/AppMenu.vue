<template>
  <nav class="app-nav">
    <div class="nav-inner">
      <router-link
        v-for="menu in visibleMenus"
        :key="menu.path"
        :to="menu.path"
        class="nav-item"
        :class="{ active: currentRoute === menu.path }"
      >
        <el-icon class="nav-icon"><component :is="menu.icon" /></el-icon>
        <span class="nav-text">{{ menu.title }}</span>
      </router-link>
    </div>
  </nav>
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
const currentRoute = computed(() => route.path)

// 菜单配置
const menuConfig = [
  {
    path: '/actuators',
    title: '设备控制',
    icon: Monitor,
    show: true
  },
  {
    path: '/sensors',
    title: '环境监测',
    icon: DataAnalysis,
    show: true
  },
  {
    path: '/video',
    title: '视频监控',
    icon: VideoCamera,
    show: true
  },
  {
    path: '/files',
    title: '文件管理',
    icon: Folder,
    show: true
  },
  {
    path: '/wifi',
    title: '网络管理',
    icon: Connection,
    show: true
  },
  {
    path: '/board',
    title: '板子',
    icon: Cpu,
    show: true
  },
  {
    path: '/ai',
    title: 'AI 助手',
    icon: ChatDotRound,
    show: true
  }
]

// 根据路由 meta 控制显示
const visibleMenus = computed(() => {
  return menuConfig.filter(menu => {
    const routeConfig = route.matched.find(r => r.path === menu.path)
    return routeConfig?.meta?.menuVisible !== false && menu.show
  })
})
</script>

<style lang="scss" scoped>
@import '@/styles/main';

.app-nav {
  width: 100%;
  padding: $spacing-sm 0;
}

.nav-inner {
  display: flex;
  align-items: center;
  gap: $spacing-xs;
  flex-wrap: wrap;
}

.nav-item {
  display: flex;
  align-items: center;
  gap: $spacing-sm;
  padding: $spacing-sm $spacing-md;
  border-radius: $border-radius-base;
  text-decoration: none;
  color: $text-regular;
  font-size: $font-size-base;
  font-weight: 500;
  transition: all 0.25s cubic-bezier(0.4, 0, 0.2, 1);
  position: relative;

  .nav-icon {
    font-size: 16px;
    opacity: 0.7;
    transition: all 0.25s ease;
  }

  .nav-text {
    letter-spacing: 0.3px;
  }

  // 底部指示器
  &::after {
    content: '';
    position: absolute;
    bottom: 0;
    left: 50%;
    transform: translateX(-50%) scaleX(0);
    width: 24px;
    height: 2px;
    background: $primary-color;
    border-radius: 2px;
    transition: transform 0.25s cubic-bezier(0.4, 0, 0.2, 1);
  }

  &:hover {
    color: $text-primary;
    background: rgba($text-primary, 0.04);

    .nav-icon {
      opacity: 1;
      transform: translateY(-1px);
    }
  }

  &.active {
    color: $primary-color;
    background: rgba($primary-color, 0.08);

    .nav-icon {
      opacity: 1;
      transform: translateY(-1px);
    }

    &::after {
      transform: translateX(-50%) scaleX(1);
    }
  }
}
</style>
