Name:           nexis
Version:        2.8.3
Release:        1%{?dist}
Summary:        Linux System Optimizer and Monitoring

License:        GPL-3.0-only
URL:            https://github.com/s4solutionsllc/Nexis
Source0:        https://github.com/s4solutionsllc/Nexis/archive/refs/tags/v%{version}.tar.gz#/nexis-%{version}.tar.gz

BuildRequires:  cmake >= 3.16
BuildRequires:  gcc-c++
BuildRequires:  ninja-build
BuildRequires:  qt6-qtbase-devel
BuildRequires:  qt6-qtcharts-devel
BuildRequires:  qt6-qtsvg-devel
BuildRequires:  qt6-qttools-devel
BuildRequires:  qt6-linguist
BuildRequires:  mesa-libGL-devel

Requires:       qt6-qtsvg
Requires:       qt6-qtbase-gui
Requires:       qt6-qt5compat
Recommends:     systemd
Recommends:     curl
Recommends:     adwaita-icon-theme

%description
Nexis is an open-source system optimizer and application monitor
that helps users manage startup applications, clean system caches,
monitor resources, and manage services and packages.

%prep
%autosetup -n Nexis-%{version}

%build
%cmake -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_TESTING=OFF
%cmake_build

%install
%cmake_install

%files
%license LICENSE
%doc README.md CHANGELOG.md
%{_bindir}/nexis
%{_datadir}/applications/nexis.desktop
%{_datadir}/icons/hicolor/*/apps/nexis.png
%{_datadir}/metainfo/io.github.s4solutionsllc.Nexis.metainfo.xml
%{_datadir}/nexis/translations/
%{_datadir}/doc/nexis/

%changelog
* Sun Jul 20 2026 Luke Simpson <luke@s4solutions.ai> - 2.8.3-1
- Initial RPM packaging for COPR
