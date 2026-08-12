import { defineConfig } from 'vite'
import vue from '@vitejs/plugin-vue'
import { resolve } from 'path'

export default defineConfig({
  plugins: [vue()],
  base: './',
  server: {
    host: '0.0.0.0',
    port: 5173,
    proxy: {
      '/api': {
        target: 'http://192.168.102.158:80',
        changeOrigin: true,
        ws: true,
        timeout: 120000,         // 请求超时 2 分钟
        proxyTimeout: 120000,    // 代理超时 2 分钟
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
        target: 'http://192.168.102.158:80',
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
