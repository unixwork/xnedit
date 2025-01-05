Name: xnedit
Summary: A Motif based GUI text editor
Version: 1.6.1
Release: 1%{?dist}
Source: https://unixwork.de/downloads/files/xnedit/xnedit-%{version}.tar.gz
URL: https://unixwork.de/xnedit/
License: GPLv2
Group: Applications/Editors
BuildRequires: motif-devel
BuildRequires: pkgconf
BuildRequires: desktop-file-utils
Requires: motif

%description
A fast and classic X11 text editor, based on NEdit, with full unicode support and antialiased text rendering.

%prep
%setup -q 

%build
make linux C_OPT_FLAGS="$RPM_OPT_FLAGS"

%install
rm -rf %{buildroot}
make install DESTDIR=%{buildroot}

%files
%{_bindir}/xnedit
%{_bindir}/xnc
%{_datadir}/icons/xnedit.png
%{_datadir}/applications/xnedit.desktop

%changelog
* Wed Nov 06 2024 Olaf Wintermann <olaf.wintermann@gmail.com>
- first version
