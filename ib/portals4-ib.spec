Name:           portals-ib
Version:        4.0
Release:        1%{?dist}
Summary:        Portals over IB and KNEM library
Group:          System Fabric Works
License:        Dual GPL / BSD
URL:            https://portals4.googlecode.com
Source0:        portals-4.0.tar.gz
BuildRoot:      %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

#BuildRequires:  
#Requires:      

%description
Portals 4 over IB/KNEM is a communication library.


%package test
Summary: Portals over IB and KNEM test suite
Group: System Environment/Libraries
Requires: %{name} = %{version}-%{release}


%description test
Test suite for Portals 4 libraries.


%prep
%setup -n portals-%{version}


%build
%configure
make %{?_smp_mflags}


%install
rm -rf $RPM_BUILD_ROOT
make DESTDIR=%{buildroot} install
# remove unpackaged files from the buildroot
rm -rf $RPM_BUILD_ROOT%{_includedir}


%files
%defattr(-,root,root,-)
%{_libdir}


%files test
%{_bindir}


%changelog
