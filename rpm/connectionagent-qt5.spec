Name:       connectionagent-qt5

Summary:    User Agent daemon
Version:    0.12.1
Release:    0
License:    LGPLv2
URL:        https://git.sailfishos.org/mer-core/connectionagent
Source0:    %{name}-%{version}.tar.bz2
Requires:   connman-qt5-declarative
Requires:   systemd
Requires:   systemd-user-session-targets
Requires:   connman >= 1.21
Requires:   mapplauncherd >= 4.1.23
BuildRequires:  pkgconfig(Qt5Core)
BuildRequires:  pkgconfig(Qt5DBus)
BuildRequires:  pkgconfig(connman-qt5)
BuildRequires:  pkgconfig(Qt5Network)
BuildRequires:  pkgconfig(Qt5Test)
BuildRequires:  pkgconfig(Qt5Qml)
BuildRequires:  pkgconfig(qt5-boostable)
BuildRequires:  pkgconfig(systemd)
Provides:   connectionagent > 0.10.1
Obsoletes:   connectionagent <= 0.7.6

# Because of systemctl-user
Requires(post):   systemd
Requires(postun): systemd

%description
Connection Agent provides multi user access to connman's User Agent.
It also provides autoconnecting features.

%package declarative
Summary:    Declarative plugin for connection agent.
Requires:   %{name} = %{version}-%{release}
Requires:   %{name} = %{version}

%description declarative
This package contains the declarative plugin for connection agent.

%package test
Summary:    auto test for connection agent.
Requires:   %{name} = %{version}-%{release}
Requires:   %{name} = %{version}

%description test
This package contains the auto tests for connection agent.

%package tracing
Summary:    Configuration for Connectionagent to enable tracing
Requires:   %{name} = %{version}-%{release}

%description tracing
Will enable tracing for Connectionagent

%prep
%setup -q -n %{name}-%{version}

%build
%qmake5
%make_build

%install
%qmake5_install


mkdir -p %{buildroot}%{_userunitdir}/user-session.target.wants
ln -s ../connectionagent.service %{buildroot}%{_userunitdir}/user-session.target.wants/

mkdir -p %{buildroot}%{_datadir}/mapplauncherd/privileges.d
install -m 644 -p connd/privileges %{buildroot}%{_datadir}/mapplauncherd/privileges.d/connectionagent

%post
if [ "$1" -ge 1 ]; then
systemctl-user daemon-reload || :
systemctl-user restart connectionagent.service || :
fi

%postun
if [ "$1" -eq 0 ]; then
systemctl-user stop connectionagent.service || :
systemctl-user daemon-reload || :
fi

%files
%defattr(-,root,root,-)
%{_bindir}/connectionagent
%{_datadir}/dbus-1/services/com.jolla.Connectiond.service
%{_datadir}/mapplauncherd/privileges.d/connectionagent
%{_sysconfdir}/dbus-1/session.d/connectionagent.conf
%{_userunitdir}/connectionagent.service
%{_userunitdir}/user-session.target.wants/connectionagent.service
%license COPYING

%files declarative
%defattr(-,root,root,-)
%{_libdir}/qt5/qml/com/jolla/connection/*

%files test
%defattr(-,root,root,-)
%{_prefix}/opt/tests/connectionagent/*

%files tracing
%defattr(-,root,root,-)
%config /var/lib/environment/nemo/70-connectionagent-tracing.conf
