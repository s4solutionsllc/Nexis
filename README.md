
<p align="center">
⚠️ <b>This is an actively maintained continuation of <a href="https://github.com/oguzhaninan/Stacer">oguzhaninan / Stacer</a>. Further releases are planned.</b>⚠️
</p>

<p align="center">
    <img src="https://raw.githubusercontent.com/lsimpsonsfdc/Stacer/native/screenshots/header.png" width="800">    
</p>
<p align="center">
  <b>Linux System Optimizer and Monitoring</b>
</p>

## Prerequisites

### Linux

Install Qt 6 development libraries and the Adwaita icon theme:

**Ubuntu / Debian:**
```bash
sudo apt install qt6-base-dev qt6-charts-dev qt6-svg-dev qt6-tools-dev-tools \
  libqt6concurrent6 adwaita-icon-theme cmake g++
```

**Fedora / RHEL:**
```bash
sudo dnf install qt6-qtbase-devel qt6-qtcharts-devel qt6-qtsvg-devel \
  qt6-linguist adwaita-icon-theme cmake gcc-c++
```

**Arch Linux:**
```bash
sudo pacman -S qt6-base qt6-charts qt6-svg qt6-tools adwaita-icon-theme cmake
```

### macOS

Install dependencies via Homebrew. The **adwaita-icon-theme** package is required for consistent icon rendering — without it, many UI icons will appear blank.

```bash
brew install qt@6 cmake adwaita-icon-theme
```

After installing, ensure Qt is in your path:
```bash
export PATH="$(brew --prefix qt@6)/bin:$PATH"
```

### Build

```bash
mkdir -p build && cd build
cmake ..
make -j$(nproc 2>/dev/null || sysctl -n hw.ncpu)
```

## Screenshots

<p align="center">
    <img src="https://raw.githubusercontent.com/lsimpsonsfdc/Stacer/native/screenshots/Stacer-2.0.1-01.png" width="700">
</p>

<p align="center">
    <img src="https://raw.githubusercontent.com/lsimpsonsfdc/Stacer/native/screenshots/Stacer-2.0.1-02.png" width="700">
</p>

<p align="center">
    <img src="https://raw.githubusercontent.com/lsimpsonsfdc/Stacer/native/screenshots/Stacer-2.0.1-03.png" width="700">
</p>

<p align="center">
    <img src="https://raw.githubusercontent.com/lsimpsonsfdc/Stacer/native/screenshots/Stacer-2.0.1-04.png" width="700">
</p>

<p align="center">
    <img src="https://raw.githubusercontent.com/lsimpsonsfdc/Stacer/native/screenshots/Stacer-2.0.1-05.png" width="700">
</p>

<p align="center">
    <img src="https://raw.githubusercontent.com/lsimpsonsfdc/Stacer/native/screenshots/Stacer-2.0.1-06.png" width="700">
</p>

<p align="center">
    <img src="https://raw.githubusercontent.com/lsimpsonsfdc/Stacer/native/screenshots/Stacer-2.0.1-07.png" width="700">
</p>

<p align="center">
    <img src="https://raw.githubusercontent.com/lsimpsonsfdc/Stacer/native/screenshots/Stacer-2.0.1-08.png" width="700">
</p>

<p align="center">
    <img src="https://raw.githubusercontent.com/lsimpsonsfdc/Stacer/native/screenshots/Stacer-2.0.1-09.png" width="700">
</p>

<p align="center">
    <img src="https://raw.githubusercontent.com/lsimpsonsfdc/Stacer/native/screenshots/Stacer-2.0.1-10.png" width="700">
</p>

<p align="center">
    <img src="https://raw.githubusercontent.com/lsimpsonsfdc/Stacer/native/screenshots/Stacer-2.0.1-11.png" width="700">
</p>

## Built With

This fork is co-authored by [Claude Code](https://claude.ai/claude-code), Anthropic's AI coding agent. Claude Code contributes to architecture decisions, feature implementation, bug fixes, CI/CD pipelines, and release engineering — working alongside the human maintainer as a pair-programming partner.

<!--

This project exists thanks to all the people who have previously contributed. [[Contribute](CONTRIBUTING.md)].</br>
Please let me know if you would like to join in the fun.
<a href="https://github.com/lsimpsonsfdc/Stacer/graphs/contributors"><img src="https://opencollective.com/Stacer/contributors.svg?width=890&button=false" /></a>


### Financial Contributors

Become a financial contributor and help us sustain our community. [[Contribute](https://opencollective.com/Stacer/contribute)]

#### Individuals

<a href="https://opencollective.com/Stacer"><img src="https://opencollective.com/Stacer/individuals.svg?width=890"></a>

#### Organizations

Support this project with your organization. Your logo will show up here with a link to your website. [[Contribute](https://opencollective.com/Stacer/contribute)]

<a href="https://opencollective.com/Stacer/organization/0/website"><img src="https://opencollective.com/Stacer/organization/0/avatar.svg"></a>
<a href="https://opencollective.com/Stacer/organization/1/website"><img src="https://opencollective.com/Stacer/organization/1/avatar.svg"></a>
<a href="https://opencollective.com/Stacer/organization/2/website"><img src="https://opencollective.com/Stacer/organization/2/avatar.svg"></a>
<a href="https://opencollective.com/Stacer/organization/3/website"><img src="https://opencollective.com/Stacer/organization/3/avatar.svg"></a>
<a href="https://opencollective.com/Stacer/organization/4/website"><img src="https://opencollective.com/Stacer/organization/4/avatar.svg"></a>
<a href="https://opencollective.com/Stacer/organization/5/website"><img src="https://opencollective.com/Stacer/organization/5/avatar.svg"></a>
<a href="https://opencollective.com/Stacer/organization/6/website"><img src="https://opencollective.com/Stacer/organization/6/avatar.svg"></a>
<a href="https://opencollective.com/Stacer/organization/7/website"><img src="https://opencollective.com/Stacer/organization/7/avatar.svg"></a>
<a href="https://opencollective.com/Stacer/organization/8/website"><img src="https://opencollective.com/Stacer/organization/8/avatar.svg"></a>
<a href="https://opencollective.com/Stacer/organization/9/website"><img src="https://opencollective.com/Stacer/organization/9/avatar.svg"></a>
-->
