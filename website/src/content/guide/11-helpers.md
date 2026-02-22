---
title: "Helpers"
description: "Use the Hosts File Manager to safely view and edit your system's hosts file."
order: 11
icon: "wrench"
---

# Helpers

The Helpers page contains utility tools that don't fit neatly into the other categories. Currently, its main feature is the **Hosts File Manager** -- a graphical editor for the `/etc/hosts` file that saves you from editing a sensitive system file by hand in a terminal.

![Helpers page with Hosts File Manager](/Nexis/images/guide/helpers-hosts.png)

## What is the Hosts File?

The hosts file (`/etc/hosts`) maps hostnames to IP addresses on your local machine. When your computer tries to reach a hostname, it checks this file before querying DNS. Common uses include:

- **Blocking unwanted domains** by pointing them to `127.0.0.1`
- **Setting up local development** aliases (e.g., mapping `myapp.local` to `127.0.0.1`)
- **Overriding DNS** for testing or debugging

Editing this file manually can be error-prone. A misplaced character can break name resolution for your entire system. The Hosts File Manager gives you a structured, validated interface for making changes safely.

## Viewing Entries

When you navigate to the Helpers page, Nexis reads and parses your current hosts file. Each entry appears as a row showing the **IP address**, **hostname**, and any **aliases**.

> **Tip:** The hosts file is lazy-loaded -- Nexis only reads it when you actually visit this page, so it does not add overhead during normal use.

## Adding an Entry

Click the **Add** button to create a new entry. You need to provide:

- **IP Address** -- An IPv4 address (like `127.0.0.1`) or an IPv6 address (like `::1`).
- **Hostname** -- The domain name to associate with that IP address (e.g., `myapp.local`).
- **Aliases** (optional) -- Additional hostnames that should resolve to the same address.

Nexis validates each field as you type:

- IP addresses are validated using standard IPv4 and IPv6 rules.
- Hostnames are checked against RFC 1123 formatting rules (letters, digits, hyphens, and dots; underscores are tolerated for compatibility).
- Each alias is validated individually using the same hostname rules.

If any field has an error, you will see a clear message explaining what needs to be fixed before you can save.

## Editing an Entry

Select an existing entry and click **Edit** to modify its IP address, hostname, or aliases. The same validation rules apply. This is safer than hand-editing the file because the validation catches common mistakes like malformed IP addresses or invalid characters in hostnames.

## Deleting an Entry

Select one or more entries and click **Delete** to remove them. The entries are not removed from disk until you save.

## Saving Changes

When you click **Save**, Nexis shows a **confirmation dialog** that summarizes exactly what will change -- how many entries are being added, modified, and deleted. Review this summary before confirming.

### Automatic Backup

Before writing any changes, Nexis creates a backup of your current hosts file at:

```
/etc/hosts.nexis-backup
```

The backup preserves the original file's permissions. If something goes wrong, you can restore it from this path.

### How Writes Work

Because the hosts file is owned by root, Nexis needs administrator privileges to save changes. It writes the new content securely using `sudo tee`, piping the data through stdin rather than using a temporary file. You will be prompted for your password (or Touch ID on macOS) when saving.

> **macOS:** The administrator prompt appears as a native macOS dialog asking for your password.

> **Linux:** The prompt uses `pkexec` or a terminal-based `sudo` prompt, depending on your desktop environment.

### Error Handling

If you cancel the authentication prompt, Nexis tells you the save was cancelled -- no changes are written. If the write fails for any other reason (permissions, disk error), an error dialog explains what happened. A success message appears in the status area when the save completes normally.

## What's Next

Manage your package repositories on the [APT / Homebrew](./12-apt-homebrew) page.
