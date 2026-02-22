import { defineCollection, z } from 'astro:content';

const guideCollection = defineCollection({
  type: 'content',
  schema: z.object({
    title: z.string(),
    description: z.string(),
    order: z.number(),
    icon: z.string().optional(),
    platform: z.enum(['all', 'linux', 'macos']).default('all'),
  }),
});

export const collections = {
  guide: guideCollection,
};
