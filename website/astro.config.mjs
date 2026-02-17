import { defineConfig } from 'astro/config';
import tailwind from '@astrojs/tailwind';

export default defineConfig({
  site: 'https://lsimpsonsfdc.github.io',
  base: '/Nexis',
  integrations: [tailwind()],
});
