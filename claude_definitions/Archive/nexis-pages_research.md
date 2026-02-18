# Nexis GitHub Pages Site — Research

## 1. GitHub Pages Hosting Options

### Option A: `docs/` Folder on the `native` Branch

**How it works:** A `/docs` folder at the repository root on the `native` branch. GitHub Pages is configured in Settings > Pages to serve from the `native` branch, `/docs` folder.

**Pros:**
- Site source lives alongside application code — single branch, single PR workflow.
- Every code change and site change go through the same review process.
- No orphan branches to manage; simpler mental model for contributors.
- Works immediately with GitHub's built-in Jekyll processing (no Actions required for basic Jekyll sites).

**Cons:**
- Pollutes the application codebase with website files (HTML/CSS/JS, node_modules if using a build tool).
- Every site-related commit shows up in the main development history.
- For SSGs other than Jekyll, you still need GitHub Actions to build — the `docs/` folder would contain built output (committing build artifacts is bad practice).
- The `docs/` folder name is misleading if it contains a marketing site rather than documentation.

### Option B: Separate `gh-pages` Branch

**How it works:** A dedicated orphan branch (`gh-pages`) contains only the built site output. A GitHub Actions workflow builds the site from source files and pushes to this branch.

**Pros:**
- Clean separation between application code and website.
- No build artifacts in the development branch history.
- Standard convention that contributors and tools recognize.

**Cons:**
- Requires GitHub Actions to build and deploy.
- An extra branch to be aware of.

### Option C: GitHub Actions Deployment (Recommended)

**How it works:** GitHub Pages is configured to use "GitHub Actions" as the build and deployment source. The workflow builds the site and deploys using the official `actions/upload-pages-artifact` and `actions/deploy-pages` actions. No `gh-pages` branch is needed.

**Pros:**
- Cleanest approach — no `gh-pages` branch, no `docs/` folder with build artifacts.
- Site source files can live in a directory like `/website` on the `native` branch.
- Full control over the build process.
- Works with any SSG (Hugo, Astro, plain HTML, etc.).
- GitHub's official recommended approach as of 2025/2026.
- Environment protection rules can restrict which branches are allowed to deploy.

**Cons:**
- Requires writing a workflow file (though GitHub provides starter templates).
- Slightly more setup than the `docs/` folder approach.

### Configuring with `native` as the Default Branch

Since the Nexis default branch is `native` (not `main` or `master`):

1. **For GitHub Actions deployment:** Settings > Pages → select "GitHub Actions" as the source. No branch selection needed — the workflow controls everything.
2. **Environment protection:** GitHub auto-creates a `github-pages` environment. Since `native` is the default branch, deployments should work out of the box. If not, Settings > Environments > github-pages → update deployment branch rules.
3. **For branch-based deployment:** Any branch can be selected from the dropdown, including `native`.

---

## 2. Static Site Generator Comparison

### Jekyll

- **Build complexity:** Low. GitHub has built-in Jekyll support.
- **GitHub Actions integration:** Starter workflow available. Built-in support handles most cases.
- **Theme ecosystem:** Huge (1,500+ themes). Many are dated or blog-focused.
- **Maintenance burden:** Low once set up. Ruby dependency can be annoying for non-Ruby developers.
- **Performance:** Slow builds (30-60s for ~1,000 pages). Fine for a small landing page.
- **Best for:** Simple documentation sites, blogs. Not ideal for a polished landing page with custom design.

### Hugo

- **Build complexity:** Medium. Single Go binary, no runtime dependencies.
- **GitHub Actions integration:** Excellent. Official starter workflow at `actions/starter-workflows/pages/hugo.yml`. Uses `peaceiris/actions-hugo`.
- **Theme ecosystem:** Large (400+ themes). Many modern, well-designed themes.
- **Maintenance burden:** Very low. Single binary, no dependency tree. Strong backward compatibility.
- **Performance:** Fastest SSG available. Sub-second builds.
- **Best for:** Documentation-heavy sites, blogs, project sites. Good balance of power and simplicity.

### Astro

- **Build complexity:** Medium-high. Node.js ecosystem (npm/pnpm). Excellent defaults.
- **GitHub Actions integration:** Excellent. Official starter workflow at `actions/starter-workflows/pages/astro.yml`. Uses `withastro/action`.
- **Theme ecosystem:** Growing rapidly (100+ themes). Modern, component-based. Islands architecture ships zero JavaScript by default.
- **Maintenance burden:** Medium. Node.js dependencies need periodic updates. Actively maintained and stable.
- **Performance:** Ships zero JS by default — pages load extremely fast. Build times good (faster than Jekyll, slower than Hugo).
- **Best for:** Modern, interactive landing pages. Component-based development. When you want React/Vue/Svelte components without shipping a JS framework to the client.
- **Key consideration:** Requires setting `base: '/Nexis'` in `astro.config.mjs` for project sites (unless using a custom domain).

### Next.js Static Export

- **Build complexity:** High. Full React framework, even for static output. Overkill for a project landing page.
- **GitHub Actions integration:** Possible but not as streamlined.
- **Theme ecosystem:** Few static-site-specific themes. Most assume SSR.
- **Maintenance burden:** High. Large dependency tree. Frequent major version updates.
- **Performance:** Heavier due to React runtime overhead.
- **Best for:** Full web applications. Not recommended for a project landing page.

### Plain HTML/CSS/JS

- **Build complexity:** Zero. No build step needed.
- **GitHub Actions integration:** Trivial — just upload the files.
- **Theme ecosystem:** None (but any CSS framework like Tailwind works).
- **Maintenance burden:** Lowest possible. No dependencies to update.
- **Performance:** As fast as it gets.
- **Best for:** Simple landing pages where you want total control and zero maintenance.

### Summary Table

| Generator | Build Complexity | JS Shipped | Theme Ecosystem | Maintenance | Best For |
|---|---|---|---|---|---|
| Jekyll | Low | None | Huge (dated) | Low (Ruby) | Blogs/docs |
| Hugo | Medium | None | Large (400+) | Very low | Docs-heavy sites |
| **Astro** | **Medium** | **Zero by default** | **Growing (100+)** | **Medium** | **Modern landing pages** |
| Next.js | High | Heavy | Few static | High | Full web apps |
| Plain HTML | Zero | None | None | Lowest | Simple pages |

### Recommendation: Astro

1. Nexis needs a polished, modern landing page — not a blog or docs site. Astro excels at component-based marketing pages.
2. Zero-JS-by-default means the site loads instantly, matching the performance expectations of a system optimizer tool.
3. Tailwind CSS integration is first-class.
4. Official GitHub Actions starter workflow simplifies deployment.
5. Component-based architecture makes the page maintainable and modular.
6. Growing ecosystem with modern themes and templates.

---

## 3. Exemplary Open-Source Desktop App Websites

### Alacritty (alacritty.org)

**Technology:** Hand-written HTML, hosted on GitHub Pages.

**Structure:**
- Header with logo + minimal nav (Configuration, Changelog, GitHub)
- Primarily documentation-focused rather than marketing
- No hero screenshot or flashy visuals — just brand identity and description

**Design Philosophy:** Extremely minimalist, mirroring the terminal's own ethos. Simple SVG logo, hand-built CSS, no framework.

**Downloads:** Handled via GitHub Releases (links out).

**Takeaway:** Clean and on-brand, but too sparse for a feature-rich app like Nexis. Works for a single-purpose terminal emulator, not a multi-page system tool.

### Hyper (hyper.is)

**Technology:** Next.js, deployed on Vercel.

**Structure:**
- Hero section with bold branding and dark dramatic design
- Terminal screenshot/demo area
- Plugin/theme showcase section
- Download section with platform-specific buttons (likely auto-detects OS)
- Theme store at hyper.is/themes

**Design Philosophy:** Fully dark theme (black/dark gray backgrounds), vibrant accent colors, modern sleek aesthetic matching the product itself.

**Takeaway:** Strong visual identity. Platform-specific download CTAs and theme showcase are excellent patterns. Dark design aligns well with Nexis's dark mode.

### Lapce (lap.dev/lapce/)

**Technology:** Custom build, hosted at parent company domain.

**Structure:**
- Hero with strong tagline: "Lightning-fast and Powerful Code Editor"
- Feature cards highlighting key differentiators (native GUI + GPU, remote dev, tree-sitter)
- Each feature explains the user benefit concisely
- Download buttons for multiple platforms

**Design Philosophy:** Clean, modern, professional. Performance framed as a promise. Feature cards answer "how does this benefit me?"

**Takeaway:** Feature cards with benefit-oriented copy are an effective pattern. The "any lag will be treated as a bug" messaging creates trust.

### WezTerm (wezterm.org)

**Technology:** Likely MkDocs with Material theme.

**Structure:**
- Landing page with hero and feature highlights
- Extensive documentation site with sidebar navigation
- Dedicated features page
- Screenshot of WezTerm on macOS prominently displayed

**Design Philosophy:** Documentation-heavy approach. Very comprehensive — essentially a documentation site that doubles as the project homepage.

**Takeaway:** Good for doc-heavy projects, but too utilitarian for a marketing/branding site. Nexis should separate the landing page from docs.

### Flameshot (flameshot.org)

**Technology:** Zola (Rust-based SSG), hosted on GitHub Pages.

**Structure:**
- Hero section with branding and primary CTA
- Features section showcasing built-in editing capabilities (arrow marks, highlighting, blurring, text, drawing)
- **Tabbed download section** organized by OS (Linux, Windows, macOS) with package-specific buttons per tab
- Separate docs area at flameshot.org/docs/

**Design Philosophy:** Clean, modern. Features demonstrated visually. "Powerful yet simple to use" messaging.

**Takeaway:** The tabbed OS-specific download section is exactly what Nexis needs. Visual feature demonstrations with annotated screenshots are highly effective.

---

## 4. Common Patterns Across Successful Sites

### Must-Have Sections (in order)

1. **Hero** — Bold headline, one-line description, primary CTA ("Download for macOS/Linux"), secondary CTA ("View on GitHub"). Product screenshot immediately below.
2. **Features grid** — 4-6 key features with icons and short descriptions. Answer "how does this benefit me?" within 4 seconds.
3. **Screenshots/visual showcase** — 3-5 curated screenshots showing the app in action.
4. **Platform downloads** — OS-specific tabs (Linux .deb/.AppImage, macOS .dmg) with version number.
5. **Installation instructions** — Build from source commands for developers.
6. **Community/Contributing** — Links to issues, translations, changelog.
7. **Footer** — License, GitHub link, attribution.

### Design Patterns That Work

- Dark theme matching the app's own dark mode (Nexis orange `#E95420` as accent)
- Centered hero composition with product screenshot below headline
- Two CTAs in hero: bold primary ("Download") + outlined secondary ("GitHub")
- Clean typography with generous whitespace
- Mobile-first responsive design
- Avoid generic "Get started" language — use specific CTAs like "Download for macOS"

### Evil Martians Study: "100 Dev Tool Landing Pages"

Evil Martians analyzed 100+ successful devtool landing pages (Linear, Supabase, Vercel, etc.) and published their findings:

- **Hero section:** Most follow centered composition — big bold headline in the middle, supporting graphic below. Stable, trustworthy, works.
- **Dual CTAs:** Primary (bold, specific) + secondary (outlined, "View docs" / "GitHub"). Never generic "Get started."
- **Credibility section:** Clients/users/stars section right after hero is the fastest way to build trust.
- **Feature demos:** Deliver main value in first 4 seconds, keep the loop under 8 seconds.
- **Design philosophy:** Landing pages convert thanks to the right block sequence and predictable user journey — not animations. Fewer mental steps = higher results.
- **Mobile-first:** Design for mobile first, where users have one screen and a few seconds.

**LaunchKit:** Evil Martians created a free open-source landing page template for devtools based on this research. Built with Astro, deploys to GitHub Pages, customizable via CSS variables. Available at:
- Demo: https://launchkit.evilmartians.io/
- Source: https://github.com/evilmartians/devtool-template

---

## 5. GitHub Actions Deployment

### Official Approach (2025/2026)

GitHub provides starter workflow templates for all major SSGs. The Astro template uses:

```yaml
name: Deploy Astro site to Pages
on:
  push:
    branches: ["native"]
    paths: ["website/**"]
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
      - uses: actions/checkout@v4
      - uses: withastro/action@v3
        with:
          path: ./website
      - uses: actions/upload-pages-artifact@v3
  deploy:
    needs: build
    runs-on: ubuntu-latest
    environment:
      name: github-pages
      url: ${{ steps.deployment.outputs.page_url }}
    steps:
      - id: deployment
        uses: actions/deploy-pages@v4
```

Key points:
- `withastro/action` handles Node.js setup, dependency install, and build
- `upload-pages-artifact` uploads the built site
- `deploy-pages` publishes to GitHub Pages
- `paths: ["website/**"]` ensures the workflow only runs when site files change (not on C++ code changes)
- Environment protection rules restrict deployment to the default branch

---

## 6. Custom Domain Considerations

### Without Custom Domain

The site is accessible at `lsimpsonsfdc.github.io/Nexis/`. This requires:
- Setting `base: '/Nexis'` in `astro.config.mjs`
- All internal links and asset paths must account for the base path
- Works immediately, no DNS needed

### With Custom Domain

A custom domain (e.g., `nexis.dev`, `nexisapp.com`) adds credibility:

**Setup:**
1. Purchase the domain from any registrar
2. Add a DNS record:
   - For apex domain (`nexis.dev`): A records pointing to GitHub's IPs (185.199.108-111.153)
   - For subdomain (`www.nexis.dev`): CNAME record → `lsimpsonsfdc.github.io`
3. In GitHub repo Settings > Pages, enter the custom domain
4. GitHub auto-provisions an SSL certificate via Let's Encrypt
5. Add a `CNAME` file to the deploy output containing the domain name

**Can be added later** without breaking anything — the GitHub Pages URL continues to work and redirects to the custom domain.

---

## 7. SEO & Discoverability

### Technical SEO

- **Open Graph tags:** `og:title`, `og:description`, `og:image` (use a branded social preview card)
- **Meta description:** Concise description for search results
- **Canonical URL:** Prevents duplicate content issues
- **Sitemap.xml:** Astro generates this automatically with `@astrojs/sitemap`
- **robots.txt:** Allow all crawlers

### GitHub Integration

- Set the repo's "Website" field (Settings > General) to the Pages URL
- This makes the link prominent on the repository page
- The GitHub social preview image (`og:image`) will be used when sharing the repo

### Content Strategy

- Use semantic HTML (`<h1>`, `<h2>`, `<article>`, `<nav>`, `<footer>`)
- Include keywords naturally: "Linux system optimizer", "macOS system monitor", "GPU monitoring", "system cleaner"
- Alt text on all screenshots
- Fast load times (Astro's zero-JS helps here)

---

## 8. Existing Assets

The project already has significant assets ready for the website:

- **Header image:** `screenshots/header.png` — branded header graphic
- **Screenshots:** 18 app screenshots (`Nexis-01.png` through `Nexis-18.png`)
- **App icon:** SVG logo available in the codebase
- **Color palette:** Nexis orange `#E95420`, dark theme colors from `style.qss`
- **Feature list:** Comprehensive list in README.md and FEATURES.md
- **Changelog:** CHANGELOG.md for release history
- **Build instructions:** Already documented in README.md

---

## Sources

- [GitHub Actions starter workflows (Astro)](https://github.com/actions/starter-workflows/blob/main/pages/astro.yml)
- [GitHub Actions starter workflows (Hugo)](https://github.com/actions/starter-workflows/blob/main/pages/hugo.yml)
- [Deploy Astro to GitHub Pages](https://docs.astro.build/en/guides/deploy/github/)
- [LaunchKit — free devtool landing page template](https://launchkit.evilmartians.io/)
- [LaunchKit GitHub repo](https://github.com/evilmartians/devtool-template)
- [Evil Martians: 100 dev tool landing pages study](https://evilmartians.com/chronicles/we-studied-100-devtool-landing-pages-here-is-what-actually-works-in-2025)
- [Evil Martians: 3 smart ways to highlight features](https://evilmartians.com/chronicles/three-smart-ways-to-highlight-features-for-landing-pages-or-launch-weeks)
- [Alacritty website source](https://github.com/alacritty/website)
- [Hyper website](https://hyper.is)
- [Lapce website](https://lap.dev/lapce/)
- [WezTerm website](https://wezterm.org)
- [Flameshot website source](https://github.com/flameshot-org/flameshot-org.github.io)
- [peaceiris/actions-gh-pages](https://github.com/peaceiris/actions-gh-pages)
- [Ghostty](https://ghostty.org)
