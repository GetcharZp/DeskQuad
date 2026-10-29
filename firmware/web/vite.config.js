import { fileURLToPath, URL } from 'node:url';
import { defineConfig } from 'vite';
import vue from '@vitejs/plugin-vue';

// 开发时把所有后端接口代理到真实设备，前端代码始终使用相对路径
const device = process.env.VITE_DEVICE_URL || 'http://192.168.4.1';
const apiPattern = '^/api(?:[/?]|$)';

export default defineConfig({
  plugins: [vue()],
  base: '/',
  resolve: {
    alias: {
      '@': fileURLToPath(new URL('./src', import.meta.url)),
    },
  },
  define: {
    __VUE_PROD_DEVTOOLS__: 'false',
  },
  server: {
    host: true,
    proxy: {
      [apiPattern]: {
        target: device,
        changeOrigin: true,
      },
    },
  },
  build: {
    target: 'es2016',
    cssCodeSplit: false,
    sourcemap: false,
    assetsInlineLimit: 0,
    chunkSizeWarningLimit: 700,
    rollupOptions: {
      output: {
        entryFileNames: 'assets/app.[hash:8].js',
        chunkFileNames: 'assets/app.[hash:8].js',
        assetFileNames: 'assets/app.[hash:8][extname]',
      },
    },
  },
});
