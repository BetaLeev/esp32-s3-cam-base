import { createRouter, createWebHashHistory } from 'vue-router'

const routes = [
  {
    path: '/',
    redirect: '/actuators'
  },
  {
    path: '/actuators',
    name: 'Actuators',
    component: () => import('@/views/actuators/index.vue'),
    meta: { title: '设备控制' }
  },
  {
    path: '/sensors',
    name: 'Sensors',
    component: () => import('@/views/sensors/index.vue'),
    meta: { title: '环境监测' }
  },
  {
    path: '/video',
    name: 'Video',
    component: () => import('@/views/video/index.vue'),
    meta: { title: '视频监控' }
  },
  {
    path: '/files',
    name: 'Files',
    component: () => import('@/views/FileManagerView.vue'),
    meta: { title: '文件管理' }
  },
  {
    path: '/files/preview',
    name: 'Preview',
    component: () => import('@/views/PreviewView.vue'),
    meta: { title: '图片预览' }
  },
  {
    path: '/files/player',
    name: 'Player',
    component: () => import('@/views/PlayerView.vue'),
    meta: { title: '媒体播放' }
  },
  {
    path: '/wifi',
    name: 'Wifi',
    component: () => import('@/views/wifi/index.vue'),
    meta: { title: 'WiFi 设置' }
  },
  {
    path: '/board',
    name: 'Board',
    component: () => import('@/views/board/index.vue'),
    meta: { title: '板子' }
  },
  {
    path: '/ai',
    name: 'AI',
    component: () => import('@/views/ai/index.vue'),
    meta: { title: 'AI 助手' }
  }
]

const router = createRouter({
  history: createWebHashHistory(),
  routes
})

router.beforeEach((to, from, next) => {
  document.title = to.meta.title ? `${to.meta.title} - ESP32` : 'ESP32 控制面板'
  next()
})

export default router
