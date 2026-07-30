Name:           usbforge
Version:        1.1.0
Release:        1%{?dist}
Summary:        Bootable ISO builder and Live USB Lab
License:        MIT
URL:            https://github.com/Kkkppmm/UBS
Source0:        usbforge-%{version}.tar.gz
BuildRequires:  gcc make pkgconfig gtk3-devel
Requires:       gtk3 xorriso
Recommends:     grub2-tools mtools

%description
USBForge creates GRUB-bootable ISOs, writes them to USB, and provides a
Live USB Lab GUI for device testing, diagnostics, and documentation.

%prep
%setup -q

%build
make %{?_smp_mflags}

%install
rm -rf %{buildroot}
install -d %{buildroot}%{_bindir}
install -d %{buildroot}%{_datadir}/usbforge/docs
install -d %{buildroot}%{_datadir}/applications
install -d %{buildroot}%{_datadir}/usbforge/scripts
install -m755 build/usbforge-builder %{buildroot}%{_bindir}/
install -m755 build/usbforge-live %{buildroot}%{_bindir}/
install -m644 docs/* %{buildroot}%{_datadir}/usbforge/docs/
install -m755 scripts/build-iso.sh %{buildroot}%{_datadir}/usbforge/scripts/
install -m755 scripts/live-autostart.sh %{buildroot}%{_datadir}/usbforge/scripts/
install -m644 packaging/linux/usbforge-builder.desktop %{buildroot}%{_datadir}/applications/
install -m644 packaging/linux/usbforge-live.desktop %{buildroot}%{_datadir}/applications/

%files
%license LICENSE
%doc README.md
%{_bindir}/usbforge-builder
%{_bindir}/usbforge-live
%{_datadir}/usbforge/
%{_datadir}/applications/usbforge-*.desktop

%changelog
* Thu Jul 30 2026 USBForge Contributors <usbforge@localhost> - 1.1.0-1
- Add Linux release packages and Windows installer support
