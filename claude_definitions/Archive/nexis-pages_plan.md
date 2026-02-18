# Nexis GitHub Pages Site — Implementation Plan

## Overview

Build a single-page landing site for Nexis using **Astro + Tailwind CSS**, deployed to GitHub Pages via GitHub Actions. Site source lives in `/website` on the `native` branch. The design follows Nexis branding (dark theme, `#E95420` orange accent) and patterns identified from studying 100+ devtool landing pages.

---

## Phase 1: Project Scaffolding

### 1.1 Initialize Astro project
- [ ] Create `/website` directory at the project root
- [ ] Run `npm create astro@latest` with the minimal/empty template inside `/website`
- [ ] Install Tailwind CSS integration: `npx astro add tailwind`
- [ ] Configure `astro.config.mjs`:
  - `site: 'https://lsimpsonsfdc.github.io'`
  - `base: '/Nexis'` (for project-site URL; remove later if custom domain added)
  - `outDir: './dist'`
- [ ] Add `@astrojs/sitemap` integration for SEO
- [ ] Create `.gitignore` in `/website` for `node_modules/`, `dist/`, `.astro/`
- [ ] Verify local dev server works: `npm run dev`

### 1.2 Configure Tailwind theme
- [ ] Define Nexis brand tokens in `tailwind.config.mjs`:
  ```js
  colors: {
    nexis: {
      orange: '#E95420',
      'orange-hover': '#c64516',
      dark: '#1a1a2e',       // page background
      'dark-card': '#16213e', // card/section background
      'dark-surface': '#0f3460', // elevated surfaces
      light: '#e4e4e4',      // primary text
      muted: '#94a3b8',      // secondary text
      border: '#334155',      // borders
    }
  }
  ```
- [ ] Configure font stack: system fonts (no custom web fonts for speed)
- [ ] Set dark background as default (no light mode toggle needed for the site)

### 1.3 Copy static assets
- [ ] Copy `screenshots/header.png` to `/website/public/images/`
- [ ] Select 5-6 best screenshots from the 18 available and copy to `/website/public/images/screenshots/`
  - Dashboard (dark mode)
  - Hardware Info page
  - System Cleaner
  - Resource Monitor with GPU chart
  - Kiosk Mode
  - Uninstaller or Services page
- [ ] Copy SVG logo to `/website/public/images/logo.svg`
- [ ] Create Open Graph social preview image (1200x630) for `og:image`

---

## Phase 2: Page Layout & Components

### 2.1 Base layout (`/website/src/layouts/Base.astro`)
- [ ] HTML head with meta tags:
  - `<title>Nexis — Linux & macOS System Optimizer</title>`
  - `<meta name="description" content="...">`
  - Open Graph tags: `og:title`, `og:description`, `og:image`, `og:url`, `og:type`
  - Twitter card meta tags
  - Canonical URL
  - Favicon (use Nexis icon)
- [ ] Dark background (`bg-nexis-dark text-nexis-light`)
- [ ] Slot for page content

### 2.2 Navigation component (`Nav.astro`)
- [ ] Fixed/sticky top bar with:
  - Nexis logo (SVG) + wordmark on the left
  - Anchor links: Features, Screenshots, Download, GitHub
  - Mobile hamburger menu for small screens
- [ ] Semi-transparent background with backdrop blur on scroll
- [ ] Smooth scroll behavior for anchor links

### 2.3 Footer component (`Footer.astro`)
- [ ] Three-column layout:
  - Column 1: Nexis logo + one-line description
  - Column 2: Links — GitHub, Releases, Issues, Changelog, Translations
  - Column 3: Built with — Qt 6, C++17, GPL v3 badge
- [ ] Attribution line: "Originally derived from Stacer by oguzhaninan"
- [ ] Copyright notice

---

## Phase 3: Page Sections

### 3.1 Hero section
- [ ] Centered composition:
  - Nexis logo/wordmark (large)
  - Headline: **"Linux & macOS System Optimizer and Monitor"**
  - Subheadline: "Monitor hardware, clean system junk, manage services — all in one app. Open source, built with Qt 6."
  - Two CTAs side by side:
    - Primary (solid orange): **"Download v1.1.2"** → GitHub Releases
    - Secondary (outlined): **"View on GitHub"** → repo link
  - Version badge below CTAs
- [ ] Hero image: `header.png` or a curated dashboard screenshot below the CTAs
- [ ] Subtle gradient or glow effect behind the hero image

### 3.2 Features section
- [ ] Section heading: "Everything you need to manage your system"
- [ ] 2×3 or 3×2 grid of feature cards, each with:
  - Icon (use simple SVG icons — Lucide or Heroicons)
  - Feature name (bold)
  - One-line description
- [ ] Features to highlight:
  1. **Real-time Dashboard** — CPU, memory, disk, GPU, and network at a glance
  2. **Hardware Info** — Detailed system, processor, graphics, and memory specs
  3. **System Cleaner** — Reclaim disk space by removing caches, logs, and trash
  4. **GPU Monitoring** — NVIDIA, AMD, Intel (Linux) and Apple Silicon (macOS)
  5. **Kiosk Mode** — F11 fullscreen dashboard for dedicated monitoring displays
  6. **Cross-platform** — Native on Linux and macOS with platform-specific integrations
- [ ] Cards: dark surface background, subtle border, rounded corners, orange accent on hover

### 3.3 Screenshots section
- [ ] Section heading: "See it in action"
- [ ] Screenshot carousel or grid:
  - Option A: Horizontal scrollable gallery with thumbnail navigation
  - Option B: 2-column masonry grid with lightbox on click
  - Option C: Featured screenshot large, thumbnails below (recommended)
- [ ] Each screenshot has a caption describing the page shown
- [ ] Subtle shadow/frame around screenshots to set them apart from the dark background

### 3.4 Platform download section
- [ ] Section heading: "Download Nexis"
- [ ] Two platform cards side by side:
  - **Linux card:**
    - Linux penguin icon
    - `.deb` download button (primary)
    - `.AppImage` download button (secondary)
    - "or build from source" link
  - **macOS card:**
    - Apple icon
    - `.dmg` download button (primary)
    - "Requires macOS 12+ (Apple Silicon)" note
    - "or build from source" link
- [ ] Links point to GitHub Releases `latest` (auto-resolves to current version)
- [ ] Version number and release date displayed

### 3.5 Build from source section
- [ ] Collapsible/accordion or separate subsection
- [ ] Tabbed code blocks: Ubuntu/Debian, Fedora, Arch, macOS
- [ ] Copy-to-clipboard button on code blocks
- [ ] Content pulled from existing README build instructions

### 3.6 Community / Contributing section
- [ ] Section heading: "Get involved"
- [ ] Three cards:
  1. **Report bugs** — Link to GitHub Issues + BUGS.md
  2. **Translate** — Link to Crowdin + note about 34 languages
  3. **Contribute code** — Link to FEATURE_REQUESTS.md + "open a PR"
- [ ] GitHub stars badge / contributor count

### 3.7 Background / About section
- [ ] Brief origin story (adapted from README Background section)
- [ ] "Stacer laid the foundation; Nexis is where it goes from here."
- [ ] Claude Code co-authorship mention with link

---

## Phase 4: GitHub Actions Workflow

### 4.1 Create deploy workflow
- [ ] Create `.github/workflows/pages.yml`:

```yaml
name: Deploy Nexis site to GitHub Pages

on:
  push:
    branches: ["native"]
    paths: ["website/**"]
  workflow_dispatch:

permissions:
  contents: read
  pages: write
  id-token: write

concurrency:
  group: "pages"
  cancel-in-progress: false

jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - name: Checkout
        uses: actions/checkout@v4

      - name: Build with Astro
        uses: withastro/action@v3
        with:
          path: ./website

  deploy:
    needs: build
    runs-on: ubuntu-latest
    environment:
      name: github-pages
      url: ${{ steps.deployment.outputs.page_url }}
    steps:
      - name: Deploy to GitHub Pages
        id: deployment
        uses: actions/deploy-pages@v4
```

### 4.2 Configure GitHub Pages
- [ ] Go to Settings > Pages
- [ ] Set source to "GitHub Actions"
- [ ] Verify `github-pages` environment allows `native` branch deployment

---

## Phase 5: SEO & Polish

### 5.1 SEO
- [ ] Verify Open Graph meta tags render correctly (use ogp.me validator)
- [ ] Verify sitemap.xml is generated at `/Nexis/sitemap-index.xml`
- [ ] Add `robots.txt` allowing all crawlers
- [ ] Set repo "Website" field to the Pages URL in GitHub Settings > General

### 5.2 Performance
- [ ] Run Lighthouse audit — target 95+ on all categories
- [ ] Verify zero JavaScript shipped (Astro default)
- [ ] Optimize images: convert screenshots to WebP with PNG fallback
- [ ] Lazy-load below-fold images

### 5.3 Responsive design
- [ ] Test on mobile (375px), tablet (768px), desktop (1280px+)
- [ ] Ensure hero CTAs stack vertically on mobile
- [ ] Feature grid collapses to single column on mobile
- [ ] Screenshots gallery is touch-scrollable on mobile
- [ ] Navigation collapses to hamburger menu on mobile

### 5.4 Accessibility
- [ ] All images have descriptive alt text
- [ ] Sufficient color contrast ratios (WCAG AA minimum)
- [ ] Keyboard-navigable (tab order, focus indicators)
- [ ] Semantic HTML throughout

---

## Phase 6: Custom Domain (Future / Optional)

- [ ] Purchase domain (e.g., `nexis.dev` or `nexisapp.com`)
- [ ] Configure DNS: A records for apex domain → GitHub IPs, CNAME for `www`
- [ ] Add domain in GitHub Pages settings
- [ ] Update `astro.config.mjs`: change `site` and remove `base`
- [ ] Add `CNAME` file to Astro `public/` directory
- [ ] Update all absolute URLs (og:url, canonical, etc.)

---

## File Structure

```
website/
├── public/
│   ├── images/
│   │   ├── logo.svg
│   │   ├── header.png
│   │   ├── og-preview.png          # 1200x630 social card
│   │   └── screenshots/
│   │       ├── dashboard.png
│   │       ├── hardware-info.png
│   │       ├── system-cleaner.png
│   │       ├── resource-monitor.png
│   │       ├── kiosk-mode.png
│   │       └── uninstaller.png
│   ├── favicon.ico
│   └── robots.txt
├── src/
│   ├── components/
│   │   ├── Nav.astro
│   │   ├── Footer.astro
│   │   ├── Hero.astro
│   │   ├── Features.astro
│   │   ├── Screenshots.astro
│   │   ├── Download.astro
│   │   ├── BuildFromSource.astro
│   │   ├── Community.astro
│   │   └── Background.astro
│   ├── layouts/
│   │   └── Base.astro
│   └── pages/
│       └── index.astro              # Assembles all sections
├── astro.config.mjs
├── tailwind.config.mjs
├── package.json
└── tsconfig.json
```

---

## Estimated Effort

| Phase | Description | Effort |
|---|---|---|
| Phase 1 | Scaffolding & config | ~30 min |
| Phase 2 | Layout & components | ~1 hour |
| Phase 3 | Page sections (7 sections) | ~3-4 hours |
| Phase 4 | GitHub Actions workflow | ~15 min |
| Phase 5 | SEO & polish | ~1 hour |
| Phase 6 | Custom domain (optional) | ~30 min |
| **Total** | | **~6-7 hours** |

---

## Acceptance Criteria

- [ ] Site is accessible at `lsimpsonsfdc.github.io/Nexis/`
- [ ] All 7 sections render correctly on desktop, tablet, and mobile
- [ ] Download buttons link to correct GitHub Release assets
- [ ] Lighthouse score 95+ on Performance, Accessibility, Best Practices, SEO
- [ ] Zero JavaScript shipped to the client
- [ ] GitHub Actions auto-deploys on push to `native` when `website/**` files change
- [ ] Open Graph preview renders correctly when sharing the URL
- [ ] Site loads in under 1 second on fast 3G
