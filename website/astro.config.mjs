import { defineConfig } from 'astro/config';
import tailwind from '@astrojs/tailwind';

export default defineConfig({
  site: 'https://s4solutionsllc.github.io',
  base: '/Nexis',
  integrations: [tailwind()],
});
