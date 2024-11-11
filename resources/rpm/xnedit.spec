Name: xnedit
Summary: A Motif based GUI text editor
Version: 1.5.3
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
%setup -q -n xnedit

%build
make linux C_OPT_FLAGS="$RPM_OPT_FLAGS"

%install
rm -rf %{buildroot}
mkdir -p %{buildroot}%{_bindir}
install -m 0755 source/xnedit %{buildroot}%{_bindir}/xnedit
install -m 0755 source/xnc %{buildroot}%{_bindir}/xnc

%files
%{_bindir}/xnedit
%{_bindir}/xnc

%changelog
* Wed Nov 06 2024 Olaf Wintermann <olaf.wintermann@gmail.com>
- first version
