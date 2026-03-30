# Website User Guide — Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Add a multi-page "User Guide" section to the Nexis website that documents every feature, page, and workflow in the application with screenshots, step-by-step instructions, and platform-specific notes.

**Architecture:** Astro content collections with Markdown files for guide content, rendered through a dedicated guide layout with sidebar navigation. Integrates into the existing site via a new nav link and guide landing page. Uses the existing Tailwind design system (nexis-* colors, card patterns, dark theme).

**Tech Stack:** Astro 4.x, Tailwind CSS 3.x, Markdown content collections, existing nexis-* color tokens

---

## Site Structure After Implementation

```
website/src/
├── content/
│   └── config.ts                      # Content collection schema
│   └── guide/
│       ├── 01-getting-started.md
│       ├── 02-dashboard.md
│       ├── 03-hardware-info.md
│       ├── 04-startup-apps.md
│       ├── 05-system-cleaner.md
│       ├── 06-search.md
│       ├── 07-services.md
│       ├── 08-processes.md
│       ├── 09-uninstaller.md
│       ├── 10-resources.md
│       ├── 11-helpers.md
│       ├── 12-apt-homebrew.md
│       ├── 13-docker.md
│       ├── 14-gnome-settings.md
│       ├── 15-settings.md
│       ├── 16-keyboard-shortcuts.md
│       └── 17-troubleshooting.md
├── layouts/
│   ├── Base.astro                     # (existing)
│   └── GuideLayout.astro             # New — guide page with sidebar nav
├── pages/
│   ├── index.astro                    # (existing)
│   └── guide/
│       ├── index.astro               # Guide landing page
│       └── [...slug].astro           # Dynamic route for guide pages
├── components/
│   ├── Nav.astro                      # (modified — add "Guide" link)
│   ├── Footer.astro                   # (existing, reused)
│   ├── GuideSidebar.astro            # New — guide section sidebar nav
│   ├── GuideCard.astro               # New — card for guide landing page
│   ├── PlatformNote.astro            # New — macOS/Linux platform callout
│   ├── KeyboardShortcut.astro        # New — styled kbd shortcut display
│   └── ...                            # (existing components unchanged)
└── styles/
    └── guide.css                      # Guide-specific prose/markdown styles
```

```
website/public/images/
├── guide/
│   ├── dashboard-overview.png
│   ├── dashboard-kiosk.png
│   ├── dashboard-disk-selector.png
│   ├── hardware-info.png
│   ├── startup-apps.png
│   ├── system-cleaner-categories.png
│   ├── system-cleaner-results.png
│   ├── search-page.png
│   ├── services-page.png
│   ├── processes-page.png
│   ├── uninstaller.png
│   ├── resources-charts.png
│   ├── helpers-hosts.png
│   ├── apt-sources.png
│   ├── homebrew-page.png
│   ├── docker-page.png
│   ├── gnome-settings.png
│   ├── settings-page.png
│   ├── command-palette.png
│   └── sidebar-navigation.png
```

---

## Task 1: Set Up Astro Content Collections for Guide Pages

**Files:**
- Create: `website/src/content/config.ts`
- Create: `website/src/content/guide/` (directory)

**Step 1: Create the content collection config**

Create `website/src/content/config.ts`:

```ts
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
```

**Step 2: Create a placeholder guide page to verify the collection works**

Create `website/src/content/guide/01-getting-started.md`:

```markdown
---
title: "Getting Started"
description: "Install Nexis and take your first look around the interface."
order: 1
icon: "rocket"
---

# Getting Started

Placeholder content — will be written in Task 5.
```

**Step 3: Build to verify content collection resolves**

Run: `cd website && npm run build`
Expected: Build succeeds with no content collection errors.

**Step 4: Commit**

```bash
git add website/src/content/
git commit -m "feat(website): add Astro content collection for user guide"
```

---

## Task 2: Create Guide Layout and Sidebar Navigation Component

**Files:**
- Create: `website/src/layouts/GuideLayout.astro`
- Create: `website/src/components/GuideSidebar.astro`
- Create: `website/src/styles/guide.css`

**Step 1: Create the GuideSidebar component**

Create `website/src/components/GuideSidebar.astro`:

```astro
---
import { getCollection } from 'astro:content';

interface Props {
  currentSlug: string;
}

const { currentSlug } = Astro.props;
const guides = (await getCollection('guide')).sort((a, b) => a.data.order - b.data.order);
---

<aside class="hidden w-64 shrink-0 lg:block">
  <nav class="sticky top-24 space-y-1 pr-4">
    <h3 class="mb-4 text-xs font-semibold uppercase tracking-wider text-nexis-muted">
      User Guide
    </h3>
    {guides.map(guide => (
      <a
        href={`/Nexis/guide/${guide.slug}/`}
        class:list={[
          'block rounded-lg px-3 py-2 text-sm transition-colors',
          currentSlug === guide.slug
            ? 'bg-nexis-orange/10 font-medium text-nexis-orange'
            : 'text-nexis-muted hover:bg-nexis-dark-card hover:text-nexis-light',
        ]}
      >
        {guide.data.title}
      </a>
    ))}
  </nav>
</aside>
```

**Step 2: Create the GuideLayout**

Create `website/src/layouts/GuideLayout.astro`:

```astro
---
import Base from './Base.astro';
import Nav from '../components/Nav.astro';
import Footer from '../components/Footer.astro';
import GuideSidebar from '../components/GuideSidebar.astro';
import '../styles/guide.css';

interface Props {
  title: string;
  description: string;
  slug: string;
}

const { title, description, slug } = Astro.props;
---

<Base title={`${title} — Nexis User Guide`} description={description}>
  <Nav />
  <div class="mx-auto flex max-w-6xl gap-8 px-6 pt-24 pb-20">
    <GuideSidebar currentSlug={slug} />
    <article class="guide-prose min-w-0 flex-1">
      <slot />
    </article>
  </div>
  <Footer />
</Base>
```

**Step 3: Create guide-specific prose styles**

Create `website/src/styles/guide.css`:

```css
/* Prose styling for guide markdown content */
.guide-prose h1 {
  @apply text-3xl font-bold text-white mb-6;
}
.guide-prose h2 {
  @apply text-2xl font-semibold text-white mt-12 mb-4 pb-2 border-b border-nexis-border;
}
.guide-prose h3 {
  @apply text-xl font-semibold text-white mt-8 mb-3;
}
.guide-prose p {
  @apply text-nexis-muted leading-relaxed mb-4;
}
.guide-prose ul, .guide-prose ol {
  @apply text-nexis-muted mb-4 pl-6 space-y-2;
}
.guide-prose ul { @apply list-disc; }
.guide-prose ol { @apply list-decimal; }
.guide-prose li { @apply leading-relaxed; }
.guide-prose code {
  @apply bg-nexis-dark-card text-nexis-orange px-1.5 py-0.5 rounded text-sm font-mono;
}
.guide-prose pre {
  @apply bg-nexis-dark-card border border-nexis-border rounded-lg p-4 mb-4 overflow-x-auto;
}
.guide-prose pre code {
  @apply bg-transparent p-0 text-nexis-light;
}
.guide-prose img {
  @apply rounded-xl border border-nexis-border shadow-lg my-6;
}
.guide-prose strong {
  @apply text-white font-semibold;
}
.guide-prose a {
  @apply text-nexis-orange hover:text-nexis-orange-hover underline;
}
.guide-prose blockquote {
  @apply border-l-4 border-nexis-orange/50 bg-nexis-dark-card rounded-r-lg px-4 py-3 my-4 text-nexis-muted;
}
.guide-prose table {
  @apply w-full border-collapse mb-4;
}
.guide-prose th {
  @apply text-left text-xs font-semibold uppercase tracking-wider text-nexis-muted border-b border-nexis-border px-3 py-2;
}
.guide-prose td {
  @apply text-sm text-nexis-muted border-b border-nexis-border/50 px-3 py-2;
}
.guide-prose kbd {
  @apply inline-block bg-nexis-dark-surface border border-nexis-border rounded px-1.5 py-0.5 text-xs font-mono text-nexis-light shadow-sm;
}
```

**Step 4: Commit**

```bash
git add website/src/layouts/GuideLayout.astro website/src/components/GuideSidebar.astro website/src/styles/guide.css
git commit -m "feat(website): add guide layout with sidebar navigation and prose styles"
```

---

## Task 3: Create Guide Page Routes and Landing Page

**Files:**
- Create: `website/src/pages/guide/index.astro`
- Create: `website/src/pages/guide/[...slug].astro`
- Create: `website/src/components/GuideCard.astro`
- Modify: `website/src/components/Nav.astro` (add "Guide" link)

**Step 1: Create the GuideCard component for the landing page**

Create `website/src/components/GuideCard.astro`:

```astro
---
interface Props {
  title: string;
  description: string;
  href: string;
  icon?: string;
}

const { title, description, href } = Astro.props;
---

<a href={href} class="group block rounded-xl border border-nexis-border bg-nexis-dark-card p-5 transition-all hover:border-nexis-orange/50 hover:shadow-lg">
  <h3 class="text-base font-semibold text-white group-hover:text-nexis-orange transition-colors">
    {title}
  </h3>
  <p class="mt-2 text-sm text-nexis-muted line-clamp-2">{description}</p>
</a>
```

**Step 2: Create the guide landing page**

Create `website/src/pages/guide/index.astro`:

```astro
---
import Base from '../../layouts/Base.astro';
import Nav from '../../components/Nav.astro';
import Footer from '../../components/Footer.astro';
import GuideCard from '../../components/GuideCard.astro';
import { getCollection } from 'astro:content';

const guides = (await getCollection('guide')).sort((a, b) => a.data.order - b.data.order);
---

<Base title="User Guide — Nexis" description="Complete guide to using Nexis, the Linux & macOS system optimizer.">
  <Nav />
  <main class="mx-auto max-w-6xl px-6 pt-24 pb-20">
    <div class="text-center mb-14">
      <h1 class="text-4xl font-bold text-white">User Guide</h1>
      <p class="mt-4 text-lg text-nexis-muted max-w-2xl mx-auto">
        Everything you need to know about using Nexis to monitor, optimize, and manage your system.
      </p>
    </div>

    <div class="grid gap-4 sm:grid-cols-2 lg:grid-cols-3">
      {guides.map(guide => (
        <GuideCard
          title={guide.data.title}
          description={guide.data.description}
          href={`/Nexis/guide/${guide.slug}/`}
        />
      ))}
    </div>
  </main>
  <Footer />
</Base>
```

**Step 3: Create the dynamic slug route for individual guide pages**

Create `website/src/pages/guide/[...slug].astro`:

```astro
---
import { getCollection } from 'astro:content';
import GuideLayout from '../../layouts/GuideLayout.astro';

export async function getStaticPaths() {
  const guides = await getCollection('guide');
  return guides.map(guide => ({
    params: { slug: guide.slug },
    props: { guide },
  }));
}

const { guide } = Astro.props;
const { Content } = await guide.render();
---

<GuideLayout title={guide.data.title} description={guide.data.description} slug={guide.slug}>
  <Content />
</GuideLayout>
```

**Step 4: Add "Guide" link to Nav.astro**

Modify `website/src/components/Nav.astro` — add a Guide link to the `links` array:

```ts
const links = [
  { href: '#features', label: 'Features' },
  { href: '#screenshots', label: 'Screenshots' },
  { href: '/Nexis/guide/', label: 'Guide' },
  { href: '#download', label: 'Download' },
  { href: 'https://github.com/s4solutions/Nexis', label: 'GitHub', external: true },
];
```

**Step 5: Build and verify**

Run: `cd website && npm run build`
Expected: Guide landing page at `/Nexis/guide/` and individual pages at `/Nexis/guide/01-getting-started/`.

**Step 6: Commit**

```bash
git add website/src/pages/guide/ website/src/components/GuideCard.astro website/src/components/Nav.astro
git commit -m "feat(website): add guide landing page, dynamic routes, and nav link"
```

---

## Task 4: Create Reusable Guide Components

**Files:**
- Create: `website/src/components/PlatformNote.astro`
- Create: `website/src/components/KeyboardShortcut.astro`
- Create: `website/src/components/GuideScreenshot.astro`
- Create: `website/src/components/GuidePrevNext.astro`

**Step 1: Create PlatformNote component**

For callouts like "On macOS, this works differently..." or "Linux only".

Create `website/src/components/PlatformNote.astro`:

```astro
---
interface Props {
  platform: 'linux' | 'macos' | 'both';
}

const { platform } = Astro.props;

const config = {
  linux: { label: 'Linux', color: 'border-yellow-500/50 bg-yellow-500/5', icon: '🐧' },
  macos: { label: 'macOS', color: 'border-blue-500/50 bg-blue-500/5', icon: '🍎' },
  both: { label: 'Platform Note', color: 'border-nexis-orange/50 bg-nexis-orange/5', icon: '💡' },
};

const { label, color, icon } = config[platform];
---

<div class:list={['rounded-lg border-l-4 px-4 py-3 my-4', color]}>
  <p class="text-xs font-semibold uppercase tracking-wider text-nexis-muted mb-1">
    {icon} {label}
  </p>
  <div class="text-sm text-nexis-muted">
    <slot />
  </div>
</div>
```

**Step 2: Create KeyboardShortcut component**

For displaying keyboard shortcuts inline.

Create `website/src/components/KeyboardShortcut.astro`:

```astro
---
interface Props {
  keys: string; // e.g. "Ctrl+K" or "F11"
}

const { keys } = Astro.props;
const parts = keys.split('+');
---

<span class="inline-flex items-center gap-0.5">
  {parts.map((key, i) => (
    <>
      <kbd class="inline-block bg-nexis-dark-surface border border-nexis-border rounded px-1.5 py-0.5 text-xs font-mono text-nexis-light shadow-sm">
        {key}
      </kbd>
      {i < parts.length - 1 && <span class="text-nexis-muted text-xs">+</span>}
    </>
  ))}
</span>
```

**Step 3: Create GuideScreenshot component**

Wraps screenshots with consistent styling and optional captions.

Create `website/src/components/GuideScreenshot.astro`:

```astro
---
interface Props {
  src: string;
  alt: string;
  caption?: string;
}

const { src, alt, caption } = Astro.props;
---

<figure class="my-6">
  <img
    src={`/Nexis/images/guide/${src}`}
    alt={alt}
    class="rounded-xl border border-nexis-border shadow-lg w-full"
    loading="lazy"
  />
  {caption && (
    <figcaption class="mt-2 text-center text-xs text-nexis-muted">{caption}</figcaption>
  )}
</figure>
```

**Step 4: Create GuidePrevNext component**

Previous/Next navigation at the bottom of each guide page.

Create `website/src/components/GuidePrevNext.astro`:

```astro
---
import { getCollection } from 'astro:content';

interface Props {
  currentSlug: string;
}

const { currentSlug } = Astro.props;
const guides = (await getCollection('guide')).sort((a, b) => a.data.order - b.data.order);
const currentIndex = guides.findIndex(g => g.slug === currentSlug);
const prev = currentIndex > 0 ? guides[currentIndex - 1] : null;
const next = currentIndex < guides.length - 1 ? guides[currentIndex + 1] : null;
---

<nav class="mt-16 flex items-stretch gap-4 border-t border-nexis-border pt-6">
  {prev ? (
    <a href={`/Nexis/guide/${prev.slug}/`} class="group flex-1 rounded-lg border border-nexis-border p-4 transition-colors hover:border-nexis-orange/50">
      <span class="text-xs text-nexis-muted">Previous</span>
      <span class="block text-sm font-medium text-nexis-light group-hover:text-nexis-orange mt-1">
        ← {prev.data.title}
      </span>
    </a>
  ) : <div class="flex-1" />}
  {next ? (
    <a href={`/Nexis/guide/${next.slug}/`} class="group flex-1 rounded-lg border border-nexis-border p-4 text-right transition-colors hover:border-nexis-orange/50">
      <span class="text-xs text-nexis-muted">Next</span>
      <span class="block text-sm font-medium text-nexis-light group-hover:text-nexis-orange mt-1">
        {next.data.title} →
      </span>
    </a>
  ) : <div class="flex-1" />}
</nav>
```

**Step 5: Update GuideLayout to include prev/next**

Add `GuidePrevNext` to `GuideLayout.astro` after the `<slot />`:

```astro
<!-- After <slot /> in the article -->
<GuidePrevNext currentSlug={slug} />
```

**Step 6: Commit**

```bash
git add website/src/components/PlatformNote.astro website/src/components/KeyboardShortcut.astro website/src/components/GuideScreenshot.astro website/src/components/GuidePrevNext.astro website/src/layouts/GuideLayout.astro
git commit -m "feat(website): add reusable guide components (platform notes, shortcuts, screenshots, prev/next)"
```

---

## Task 5: Write All 17 Guide Content Pages

**Files:**
- Create: `website/src/content/guide/01-getting-started.md` through `17-troubleshooting.md`

Each markdown file uses the frontmatter schema defined in Task 1. Content should be written using the APPLICATION_OVERVIEW.md as the source of truth for feature descriptions.

**Important notes for content writing:**
- Use the `docs/APPLICATION_OVERVIEW.md` as the authoritative source for feature details, but rewrite in user-facing language (not developer docs)
- Reference screenshots by filename (Task 6 captures them) using standard markdown: `![Alt text](/Nexis/images/guide/filename.png)`
- Use `> **Platform Note (macOS):**` blockquotes for platform-specific information
- Use `` `kbd` `` tags for keyboard shortcuts
- Keep each page focused on one application page/feature
- Include a "What you'll learn" section at the top of each page
- Include practical "How to" steps, not just descriptions

### Guide pages to write:

| File | Title | Content Source |
|------|-------|---------------|
| `01-getting-started.md` | Getting Started | Installation, first launch, UI overview (sidebar, pages, tray icon) |
| `02-dashboard.md` | Dashboard | All tiles (CPU, Memory, Disk, Network, GPU, Temp, Battery), system summary, kiosk mode, command palette |
| `03-hardware-info.md` | Hardware Info | All 8 sections, what each field means |
| `04-startup-apps.md` | Startup Apps | Add/edit/delete/enable/disable, delay option, icons |
| `05-system-cleaner.md` | System Cleaner | 6 categories, scanning, cleaning, scheduled cleaning setup |
| `06-search.md` | Search | All filter options, results interaction |
| `07-services.md` | Services | Start/stop, enable/disable, status filters |
| `08-processes.md` | Processes | Process table, end process, refresh rate, search |
| `09-uninstaller.md` | Uninstaller | Package tree, multi-select, purge option, Homebrew on macOS |
| `10-resources.md` | Resources | All 7 chart types, disk usage launcher |
| `11-helpers.md` | Helpers | Hosts file editor: add/edit/delete entries, validation, backup |
| `12-apt-homebrew.md` | APT / Homebrew | Linux APT repo management, macOS Homebrew management |
| `13-docker.md` | Docker | Images, containers, volumes tabs, prune operations |
| `14-gnome-settings.md` | GNOME Settings | 4 tabs: Appearance, Window Manager, Mouse, Desktop |
| `15-settings.md` | Settings | All preferences: theme, font, language, alerts, disk analyzer |
| `16-keyboard-shortcuts.md` | Keyboard Shortcuts | Complete reference table of all shortcuts |
| `17-troubleshooting.md` | Troubleshooting | FAQ, common issues, platform-specific notes |

**Step 1: Write all 17 guide pages**

Use `docs/APPLICATION_OVERVIEW.md` sections as the content source. Rewrite each in user-facing tutorial style.

**Step 2: Build to verify all pages render**

Run: `cd website && npm run build`
Expected: 17 guide pages generated without errors.

**Step 3: Commit**

```bash
git add website/src/content/guide/
git commit -m "feat(website): write all 17 user guide content pages"
```

---

## Task 6: Capture and Add Guide Screenshots

**Files:**
- Create: `website/public/images/guide/` (directory with ~20 screenshots)

**Step 1: Capture screenshots from the running application**

Screenshots needed (capture from both dark and light themes, use dark theme for the guide):

| Filename | Content |
|----------|---------|
| `dashboard-overview.png` | Full dashboard with all tiles visible |
| `dashboard-kiosk.png` | Dashboard in kiosk mode (fullscreen) |
| `dashboard-disk-selector.png` | Disk tile with gear menu open |
| `command-palette.png` | Command palette popup (Ctrl+K) |
| `sidebar-navigation.png` | Sidebar expanded showing all sections |
| `hardware-info.png` | Hardware Info page (full view) |
| `startup-apps.png` | Startup Apps list with entries |
| `system-cleaner-categories.png` | System Cleaner category selection view |
| `system-cleaner-results.png` | System Cleaner scan results tree |
| `search-page.png` | Search page with filters |
| `services-page.png` | Services page with status indicators |
| `processes-page.png` | Processes page with running processes |
| `uninstaller.png` | Uninstaller/Applications page |
| `resources-charts.png` | Resources page with history charts |
| `helpers-hosts.png` | Hosts file editor |
| `apt-sources.png` | APT Source Manager (Linux) |
| `homebrew-page.png` | Homebrew page (macOS) |
| `docker-page.png` | Docker management page |
| `gnome-settings.png` | GNOME Settings page |
| `settings-page.png` | Settings page |

**Step 2: Optimize images**

Resize to max 1200px wide, compress PNGs (use `pngquant` or similar).

**Step 3: Commit**

```bash
git add website/public/images/guide/
git commit -m "feat(website): add user guide screenshots"
```

---

## Task 7: Add Mobile-Responsive Guide Sidebar

**Files:**
- Modify: `website/src/layouts/GuideLayout.astro`
- Modify: `website/src/components/GuideSidebar.astro`

**Step 1: Add mobile sidebar toggle**

The desktop sidebar is `hidden lg:block`. On mobile, add a hamburger-style toggle button that reveals the guide navigation as a slide-down or modal overlay.

Update `GuideLayout.astro` to include a mobile toggle button above the article:

```astro
<!-- Mobile guide nav toggle (visible below lg breakpoint) -->
<button
  id="guide-nav-toggle"
  class="mb-4 flex items-center gap-2 rounded-lg border border-nexis-border px-3 py-2 text-sm text-nexis-muted lg:hidden"
>
  <svg class="h-4 w-4" fill="none" viewBox="0 0 24 24" stroke="currentColor">
    <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M4 6h16M4 12h16M4 18h16" />
  </svg>
  Guide Navigation
</button>
```

Update `GuideSidebar.astro` to support mobile visibility:

```astro
<aside id="guide-sidebar" class="hidden w-full shrink-0 lg:block lg:w-64">
  <!-- existing nav content -->
</aside>

<script>
  const toggle = document.getElementById('guide-nav-toggle');
  const sidebar = document.getElementById('guide-sidebar');
  toggle?.addEventListener('click', () => {
    sidebar?.classList.toggle('hidden');
    sidebar?.classList.toggle('lg:hidden');
  });
</script>
```

**Step 2: Build and test at mobile viewport**

Run: `cd website && npm run dev`
Test: Resize browser to mobile width, verify toggle shows/hides guide nav.

**Step 3: Commit**

```bash
git add website/src/layouts/GuideLayout.astro website/src/components/GuideSidebar.astro
git commit -m "feat(website): add mobile-responsive guide sidebar toggle"
```

---

## Task 8: Update Homepage to Link to Guide

**Files:**
- Modify: `website/src/components/Hero.astro` (optional — add "Read the Guide" CTA)
- Modify: `website/src/components/Community.astro` (add guide card)

**Step 1: Add guide link to Community section**

Add a 4th card to the Community section (or replace one) that links to the User Guide:

```astro
{
  title: 'Read the Guide',
  description: 'Learn how to use every feature of Nexis with our detailed user guide.',
  href: '/Nexis/guide/',
  icon: '📖',
}
```

**Step 2: Build and verify**

Run: `cd website && npm run build`
Expected: Homepage Community section includes guide link.

**Step 3: Commit**

```bash
git add website/src/components/Community.astro
git commit -m "feat(website): link user guide from homepage community section"
```

---

## Task 9: Build, Deploy, and Verify

**Step 1: Full build**

Run: `cd website && npm run build`
Expected: Clean build with all guide pages generated in `dist/guide/`.

**Step 2: Preview locally**

Run: `cd website && npm run preview`
Test: Navigate to `/Nexis/guide/`, verify:
- Landing page shows all 17 guide cards
- Each card links to the correct guide page
- Sidebar navigation highlights the current page
- Previous/Next navigation works at page bottom
- Screenshots render correctly
- Mobile responsive sidebar toggle works
- All internal links resolve correctly

**Step 3: Commit and push**

```bash
git add website/
git commit -m "feat(website): complete user guide section — 17 pages with sidebar navigation"
git push
```

**Step 4: Verify GitHub Pages deployment**

Wait for CI to deploy, then verify at `https://s4solutions.github.io/Nexis/guide/`.
