---
title: "Docker"
description: "Manage Docker images, containers, and volumes with a visual interface for cleanup and control."
order: 13
icon: "container"
---

# Docker

The Docker page gives you a graphical overview of your Docker resources -- images, containers, and volumes -- without needing to remember CLI commands. You can start and stop containers, remove unused images, prune dangling resources, and search across everything from a single tabbed interface.

This page only appears in the sidebar when Nexis detects that the `docker` CLI is installed on your system. If you install Docker after launching Nexis, restart the app to pick it up.

![Docker management page](/Nexis/images/guide/docker.png)

## Before You Start

The Docker daemon must be running for this page to work. If the daemon is not active, Nexis displays an error message instead of the usual tabs. Start Docker Desktop (or run `sudo systemctl start docker` on Linux) and then revisit the page.

## The Three Tabs

The Docker page is organized into three tabs, one for each type of Docker resource. Each tab loads its data on demand -- Nexis only queries Docker when you switch to a tab, keeping things responsive.

### Images Tab

Docker images are the blueprints used to create containers. Nexis groups them into three categories:

| Group | Description |
|-------|-------------|
| **In Use** | Images that are currently referenced by at least one container |
| **Dangling** | Images that are no longer tagged and not used by any container -- safe to remove |
| **Other** | Tagged images that are not currently in use by a running or stopped container |

Each image shows its repository name, tag, size, and creation date.

#### Removing Images

Check the boxes next to the images you want to remove and click **Remove Selected**. To clean up all dangling images at once, use the **Prune** button, which is the equivalent of running `docker image prune`.

> **Tip:** Dangling images accumulate over time as you rebuild containers with updated base images. Pruning them regularly can free significant disk space.

### Containers Tab

Containers are grouped by their current state:

| Group | Description |
|-------|-------------|
| **Running** | Containers that are currently active |
| **Exited** | Containers that have stopped |
| **Paused** | Containers that have been paused |

Each container shows its name, image, status, and creation date.

#### Starting and Stopping Containers

Select a container and use the **Start** or **Stop** button to change its state. This is equivalent to `docker start` and `docker stop`. Running containers can be stopped, and exited or paused containers can be started.

#### Removing Containers

Check the boxes next to containers you want to remove and click **Remove Selected**. You can also use the **Prune** button to remove all stopped containers at once.

#### Filtering by Status

Use the status filter to show only containers in a specific state. This is helpful when you have many containers and only want to see the running ones, or only the stopped ones that might be candidates for cleanup.

### Volumes Tab

Docker volumes provide persistent storage for containers. They are grouped into:

| Group | Description |
|-------|-------------|
| **In Use** | Volumes currently mounted by at least one container |
| **Unused** | Volumes not mounted by any container -- candidates for cleanup |

#### Removing Volumes

Check the boxes next to volumes you want to remove and click **Remove Selected**. The **Prune** button removes all unused volumes, which is equivalent to `docker volume prune`.

> **Tip:** Be careful when pruning volumes. Unlike images and containers, volume data cannot be recovered once deleted. Make sure you do not need the data before removing unused volumes.

## Searching

A search bar at the top of the page filters across all three tabs. Type a name or keyword to narrow down the list. This works regardless of which tab is currently active.

## What's Next

If you are running GNOME on Linux, the next page covers how Nexis can help you tweak your desktop settings. See [GNOME Settings](./14-gnome-settings). Otherwise, skip ahead to [Settings](./15-settings) to configure Nexis itself.
