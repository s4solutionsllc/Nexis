/** @type {import('tailwindcss').Config} */
export default {
  content: ['./src/**/*.{astro,html,js,jsx,md,mdx,svelte,ts,tsx,vue}'],
  theme: {
    extend: {
      colors: {
        nexis: {
          orange: '#E95420',
          'orange-hover': '#c64516',
          dark: '#1a1a2e',
          'dark-card': '#16213e',
          'dark-surface': '#0f3460',
          light: '#e4e4e4',
          muted: '#94a3b8',
          border: '#334155',
        },
      },
      fontFamily: {
        sans: ['-apple-system', 'BlinkMacSystemFont', 'Segoe UI', 'Roboto', 'Oxygen', 'Ubuntu', 'sans-serif'],
        mono: ['ui-monospace', 'SFMono-Regular', 'Menlo', 'Monaco', 'Consolas', 'monospace'],
      },
    },
  },
  plugins: [],
};
