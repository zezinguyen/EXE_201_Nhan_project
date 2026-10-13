import { defineConfig } from 'vite'
import react from '@vitejs/plugin-react'
import zaloMiniApp from 'zmp-vite-plugin'

// https://vite.dev/config/
export default () => {
  return defineConfig({
    root: '.',
    base: '',
    plugins: [zaloMiniApp(), react()],
    build: {
      assetsInlineLimit: 0,
    },
    server: {
      proxy: {
        '/api-ai': {
          target: 'https://api.vilao.ai',
          changeOrigin: true,
          secure: false,
          rewrite: (path) => path.replace(/^\/api-ai/, ''),
        },
      },
    },
  })
}
