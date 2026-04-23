---
title: "Keyboard Shortcuts"
description: "A complete reference of all keyboard shortcuts available in Nexis."
order: 16
icon: "keyboard"
---

# Keyboard Shortcuts

Nexis includes several keyboard shortcuts that let you navigate the app and toggle features without reaching for the mouse. This page is a complete reference of every shortcut available.

## Global Shortcuts

These shortcuts work from any page in the application.

| Shortcut | Action |
|----------|--------|
| <kbd>Ctrl</kbd>+<kbd>K</kbd> | Open the **Command Palette** -- a fuzzy-search popup for jumping to any page or running common actions |
| <kbd>Ctrl</kbd>+<kbd>B</kbd> | Toggle the **sidebar** between expanded and collapsed states |
| <kbd>Ctrl</kbd>+<kbd>E</kbd> | Toggle **dashboard edit mode** (drag, resize, restyle, and recolor tiles) |
| <kbd>F11</kbd> | Toggle **kiosk mode** (fullscreen Dashboard with no sidebar or title bar) |
| <kbd>Escape</kbd> | Exit **kiosk mode** (when kiosk mode is active) |

## Using the Command Palette

The Command Palette (<kbd>Ctrl</kbd>+<kbd>K</kbd>) is the fastest way to get around. When it opens, just start typing:

- **Page names** -- Type "dash" to jump to the Dashboard, "proc" for Processes, "dock" for Docker, and so on. The fuzzy matcher is forgiving, so partial matches work.
- **Actions** -- Type "kiosk" to toggle kiosk mode, "clean" to start a cleaning scan, or other common operations.

Select a result with the arrow keys and press <kbd>Enter</kbd>, or click it directly.

> **Tip:** The Command Palette is especially useful when the sidebar is collapsed and you want to navigate quickly without expanding it first.

## Sidebar Navigation

| Shortcut | Action |
|----------|--------|
| <kbd>Ctrl</kbd>+<kbd>B</kbd> | Collapse or expand the sidebar |

When the sidebar is collapsed, it shrinks to a narrow icon rail. Page icons remain visible and clickable. Section headings are replaced by small dot indicators. Press <kbd>Ctrl</kbd>+<kbd>B</kbd> again to expand back to the full sidebar with labels.

## Dashboard Edit Mode

| Shortcut | Action |
|----------|--------|
| <kbd>Ctrl</kbd>+<kbd>E</kbd> | Enter or exit dashboard edit mode |

Dashboard edit mode lets you drag tiles to new positions, resize them, change their visual style, customize colors, and hide tiles you don't need. See the [Dashboard](./02-dashboard) guide for full details on what you can do in edit mode.

> **Note:** Edit mode and kiosk mode are mutually exclusive -- entering one exits the other.

## Kiosk Mode

| Shortcut | Action |
|----------|--------|
| <kbd>F11</kbd> | Enter or exit kiosk mode |
| <kbd>Escape</kbd> | Exit kiosk mode |

Kiosk mode hides the sidebar, title bar, and window frame to show only the Dashboard in fullscreen. This is ideal for dedicated monitoring displays. When you enter kiosk mode, a brief overlay message ("Press ESC to exit kiosk mode") fades in and out over a few seconds.

You can also toggle kiosk mode from the system tray icon's context menu or the floating button in the top-right corner of the Dashboard.

## Tips for Efficient Navigation

> **Tip:** Combine <kbd>Ctrl</kbd>+<kbd>B</kbd> with the Command Palette for a minimal-chrome workflow: collapse the sidebar to maximize content space, then use <kbd>Ctrl</kbd>+<kbd>K</kbd> whenever you need to switch pages.

> **macOS:** Qt maps <kbd>Ctrl</kbd>-labeled shortcuts to the <kbd>Cmd</kbd> (⌘) key on macOS. So <kbd>Ctrl</kbd>+<kbd>K</kbd> becomes <kbd>Cmd</kbd>+<kbd>K</kbd>, <kbd>Ctrl</kbd>+<kbd>B</kbd> becomes <kbd>Cmd</kbd>+<kbd>B</kbd>, and so on. The key labeled <kbd>Ctrl</kbd> on a Mac keyboard is not used.

## What's Next

If you run into any issues, check the [Troubleshooting](./17-troubleshooting) page for solutions to common problems.
