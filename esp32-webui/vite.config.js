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
        target: 'http://192.168.102.158',
        changeOrigin: true,
        ws: true,  // 启用 WebSocket 代理
        timeout: 0,
        proxyTimeout: 0
      },
      '/fs': {
        target: 'http://192.168.102.158',
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
