Name:           portals-ib
Version:        1.0a1
Release:        1%{?dist}
Summary:        Portals over IB and KNEM library
Group:          System Fabric Works
License:        Dual GPL / BSD
URL:            https://portals4.googlecode.com
Source0:        portals4-1.0a1.tar.bz2
BuildRoot:      %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

#BuildRequires:  
#Requires:      

%description
Portals 4 over IB/KNEM is a communication library.


%prep
%setup -n portals4-%{version}


%build
%configure --enable-fast
make %{?_smp_mflags}


%install
rm -rf $RPM_BUILD_ROOT
make DESTDIR=%{buildroot} install
# remove unpackaged files from the buildroot
rm -rf $RPM_BUILD_ROOT%{_includedir}
rm -rf $RPM_BUILD_ROOT%{_bindir}/yod


%files
%defattr(-,root,root,-)
%{_libdir}


%changelog
