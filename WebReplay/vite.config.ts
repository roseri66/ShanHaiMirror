import { defineConfig } from 'vite'
import vue from '@vitejs/plugin-vue'

// https://vite.dev/config/
export default defineConfig({
  plugins: [vue()],

  // GitHub Pages 部署在 <user>.github.io/ShanHaiMirror/ 子路径下。
  // 不设 base 的话所有资源都去根路径找，页面白屏。dev 时走 '/'，不影响开发。
  base: process.env.NODE_ENV === 'production' ? '/ShanHaiMirror/' : '/',

  server: {
    fs: {
      // 允许读取仓库根目录：内置样例直接 import `../../Docs/samples/*.json`，
      // **不在 WebReplay 里放副本**——样例是 UE 侧导出的产物，
      // 复制一份进来等于给同一份数据造第二个真源，迟早对不上。
      allow: ['..'],
    },
  },
})
