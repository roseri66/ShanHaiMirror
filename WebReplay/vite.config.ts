import { defineConfig } from 'vite'
import vue from '@vitejs/plugin-vue'

// https://vite.dev/config/
export default defineConfig(({ command }) => ({
  plugins: [vue()],

  // GitHub Pages 部署在 <user>.github.io/ShanHaiMirror/ 子路径下。
  // 不设 base 的话所有资源都去根路径找，页面白屏。
  //
  // **按 command 判断而不是 NODE_ENV**：后者在 `vite build --mode development`
  // 之类的调用下会是 'development'，base 悄悄退回 '/'，
  // 结果是本地一切正常、部署上去白屏 —— 这种错要等到线上才发现。
  base: command === 'build' ? '/ShanHaiMirror/' : '/',

  server: {
    fs: {
      // 允许读取仓库根目录：内置样例直接 import `../../Docs/samples/*.json`，
      // **不在 WebReplay 里放副本**——样例是 UE 侧导出的产物，
      // 复制一份进来等于给同一份数据造第二个真源，迟早对不上。
      allow: ['..'],
    },
  },
}))
