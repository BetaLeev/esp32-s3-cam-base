import { createRouter, createWebHashHistory } from 'vue-router'

// 菜单配置：用于控制菜单显示
export const menuConfig = [
  { path: '/actuators', title: '设备控制', icon: 'Monitor', show: true },
  { path: '/sensors', title: '环境监测', icon: 'DataAnalysis', show: true },
  { path: '/video', title: '视频监控', icon: 'VideoCamera', show: true },
  { path: '/files', title: '文件管理', icon: 'Folder', show: true },
  { path: '/wifi', title: '网络管理', icon: 'Connection', show: true },
  { path: '/board', title: '板子', icon: 'Cpu', show: true },
  { path: '/ai', title: 'AI 助手', icon: 'ChatDotRound', show: true }
]

const routes = [
  {
    path: '/',
    component: () => import('../layout/index.vue'),
    children: [
      {
        path: '',
        redirect: '/actuators'
      },
      {
        path: 'actuators',
        component: () => import('../views/actuators/index.vue'),
        meta: { menuVisible: true }
      },
      {
        path: 'sensors',
        component: () => import('../views/sensors/index.vue'),
        meta: { menuVisible: true }
      },
      {
        path: 'video',
        component: () => import('../views/video/index.vue'),
        meta: { menuVisible: true }
      },
      {
        path: 'files',
        component: () => import('../views/storage/index.vue'),
        meta: { menuVisible: true }
      },
      {
        path: 'files/preview',
        component: () => import('../views/storage/PreviewView.vue')
      },
      {
        path: 'files/player',
        component: () => import('../views/storage/PlayerView.vue')
      },
      {
        path: 'wifi',
        component: () => import('../views/wifi/index.vue'),
        meta: { menuVisible: true }
      },
      {
        path: 'board',
        component: () => import('../views/board/index.vue'),
        meta: { menuVisible: true }
      },
      {
        path: 'ai',
        component: () => import('../views/ai/index.vue'),
        meta: { menuVisible: true }
      }
    ]
  }
]

const router = createRouter({
  history: createWebHashHistory(),
  routes
})

export default router
