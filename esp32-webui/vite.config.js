import { defineConfig } from 'vite'
import vue from '@vitejs/plugin-vue'
import { resolve } from 'path'

// 定义动态主机地址常量
const TARGET_IP = '192.168.102.158';
const TARGET_HOST = `http://${TARGET_IP}`;

export default defineConfig({
  plugins: [vue()],
  base: './',
  css: {
    preprocessorOptions: {
      scss: {
        api: 'legacy',
        silenceDeprecations: ['legacy-js-api', 'import', 'global-builtin', 'color-functions']
      }
    }
  },
  server: {
    host: '0.0.0.0',
    port: 5173,
    proxy: {
      '/api': {
        target: TARGET_HOST, // 动态使用常量
        changeOrigin: true,
        ws: true,
        timeout: 600000,
        proxyTimeout: 600000,
        configure: (proxy) => {
          proxy.on('proxyReqWs', (proxyReq, req, socket) => {
            console.log('[Vite WS Proxy] 代理 WebSocket 请求')
          })
          proxy.on('error', (err, req, socket) => {
            console.error('[Vite WS Proxy] 代理错误:', err.message)
          })
          proxy.on('close', (req, socket, head) => {
            console.log('[Vite WS Proxy] 连接关闭')
          })
        }
      },
      '/fs': {
        target: TARGET_HOST, // 动态使用常量
        changeOrigin: true,
        timeout: 60000
      }
    }
  },
  build: {
    outDir: 'dist',
    assetsDir: 'assets',
    sourcemap: false,
    minify: 'esbuild'
  },
  resolve: {
    alias: {
      '@': resolve(__dirname, 'src')
    }
  }
})