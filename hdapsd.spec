Name:		hdapsd
Version:	20250908
Release:	1%{?dist}
Summary:	Protects hard drives by parking head when fall is detected

Group:		Applications/System
License:	GPLv2+
URL:		http://hdaps.sourceforge.net/
Source0:	https://github.com/linux-thinkpad/hdapsd/releases/download/%{version}/%{name}-%{version}.tar.gz
BuildRequires:	gcc
%{?systemd_requires}
BuildRequires:	systemd
BuildRequires:	libconfig-devel

%description

This is a disk protection user-space daemon. It monitors the acceleration
values through the HDAPS/AMS interfaces and automatically initiates disk head
parking if a fall or sliding of the laptop is detected.

HDAPS is typically found in ThinkPad laptops and AMS in Apple laptops.

%prep
%setup -q

%build
%configure
make %{?_smp_mflags}


%install
%make_install
install -m 644 misc/%{name}.service %{buildroot}%{_unitdir}

%files
%{_sbindir}/*
%{_udevrulesdir}/%{name}.rules
%{_unitdir}/%{name}.service
%{_unitdir}/%{name}@.service
%{_mandir}/man8/hdapsd.8.gz
%config(noreplace) %{_sysconfdir}/%{name}.conf
%doc AUTHORS
%license COPYING
%doc ChangeLog
%doc README.md


%changelog
* Wed Oct 22 2025 Evgeni Golov - 20250908-1
- New upstream release

* Tue Jul 24 2018 Adam Williamson <awilliam@redhat.com> - 20141203-10
- Rebuild for new libconfig

* Fri Jul 13 2018 Fedora Release Engineering <releng@fedoraproject.org> - 20141203-9
- Rebuilt for https://fedoraproject.org/wiki/Fedora_29_Mass_Rebuild

* Wed Feb 07 2018 Fedora Release Engineering <releng@fedoraproject.org> - 20141203-8
- Rebuilt for https://fedoraproject.org/wiki/Fedora_28_Mass_Rebuild

* Wed Aug 02 2017 Fedora Release Engineering <releng@fedoraproject.org> - 20141203-7
- Rebuilt for https://fedoraproject.org/wiki/Fedora_27_Binutils_Mass_Rebuild

* Wed Jul 26 2017 Fedora Release Engineering <releng@fedoraproject.org> - 20141203-6
- Rebuilt for https://fedoraproject.org/wiki/Fedora_27_Mass_Rebuild

* Fri Feb 10 2017 Fedora Release Engineering <releng@fedoraproject.org> - 20141203-5
- Rebuilt for https://fedoraproject.org/wiki/Fedora_26_Mass_Rebuild

* Sun Feb 14 2016 Tomasz Torcz <ttorcz@fedoraproject.org> - 20141203-4
- fix FTBFS (rhbz#1307612)

* Wed Feb 03 2016 Fedora Release Engineering <releng@fedoraproject.org> - 20141203-3
- Rebuilt for https://fedoraproject.org/wiki/Fedora_24_Mass_Rebuild

* Wed Jun 17 2015 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 20141203-2
- Rebuilt for https://fedoraproject.org/wiki/Fedora_23_Mass_Rebuild

* Wed May 06 2015 Tomasz Torcz <ttorcz@fedoraproject.org> - 20141203-1
- new upstream version

* Tue Oct 28 2014 Tomasz Torcz <ttorcz@fedoraproject.org> - 20141024-1
- new upstream version

* Sat Aug 16 2014 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 20090401.20131204git401ca60-3
- Rebuilt for https://fedoraproject.org/wiki/Fedora_21_22_Mass_Rebuild

* Sat Jun 07 2014 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 20090401.20131204git401ca60-2
- Rebuilt for https://fedoraproject.org/wiki/Fedora_21_Mass_Rebuild

* Wed Dec 04 2013 Tomasz Torcz <ttorcz@fedoraproject.org> - 20090401.20131204git401ca60c75-1
- latest upstream snapshot, fixes rhbz#1037119

* Sat Aug 03 2013 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 20090401gita64b50c-2
- Rebuilt for https://fedoraproject.org/wiki/Fedora_20_Mass_Rebuild

* Tue May 28 2013 Tomasz Torcz <ttorcz@fedoraproject.org> - 20090401gita64b50c-1
- package upstream snapshot:
  - use upstream systemd units and udev rules
  - drop sysconfig file

* Thu Feb 14 2013 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 20090401-12
- Rebuilt for https://fedoraproject.org/wiki/Fedora_19_Mass_Rebuild

* Thu Jul 19 2012 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 20090401-11
- Rebuilt for https://fedoraproject.org/wiki/Fedora_18_Mass_Rebuild

* Fri Feb 03 2012 Tomasz Torcz <ttorcz@fedoraproject.org> - 20090401-10
- spec cleanup:
  - remove BuildRoot define and it's removal from clean
  - use _unitdir macro introduced in the meantime
  - drop upstart event files
- remove StandardOutput=syslog from unit file

* Fri Jan 13 2012 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 20090401-9
- Rebuilt for https://fedoraproject.org/wiki/Fedora_17_Mass_Rebuild

* Wed Feb 09 2011 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 20090401-8
- Rebuilt for https://fedoraproject.org/wiki/Fedora_15_Mass_Rebuild

* Sun Jan 9 2011 Tomasz Torcz <ttorcz@fedoraproject.org> 20090401-7
- remove bad parentheses in ExecStart= line of systemd unit def
  (thanks Alexandre Silva Lopes, #667073)

* Sat Jul 17 2010 Tomasz Torcz <ttorcz@fedoraproject.org> 20090401-6
- provide systemd service definition
- resurrect udev rule for systemd interaction

* Mon Dec 07 2009 Tomasz Torcz <ttorcz@fedoraproject.org> 20090401-5
- port initscript file to upstart 0.6, removing custom udev rule

* Fri Sep 04 2009 Tomasz Torcz <ttorcz@fedoraproject.org> 20090401-4
- use version macro in in Source0, as per review suggestion (#505928 #9)

* Sun Jun 21 2009 Tomasz Torcz <ttorcz@fedoraproject.org> 20090401-3
- fixes from review: URL source, proper build root macro
- minor cleanup of event file

* Wed Jun 17 2009 Tomasz Torcz <ttorcz@fedoraproject.org> 20090401-2
- mark upstart event file and udev rule as config files

* Sun Jun 14 2009 Tomasz Torcz <ttorcz@fedoraproject.org> 20090401-1
- initial version
