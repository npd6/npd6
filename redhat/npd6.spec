Name:           npd6    
Version:        1.0.0
Release:        1%{?dist}
Summary:        A Linux daemon to provide a proxy service for IPv6 Neighbor Solcitations received by a gateway routing device.

Group:          Applications/Internet
License:        GNU GPL v3
URL:            https://github.com/npd6/npd6
Source0:        npd6-1.0.0.tar.gz
BuildRoot:      %(mktemp -ud %{_tmppath}/%{name}-%{version}-%{release}-XXXXXX)

#BuildRequires: 
#Requires:      

%description
npd6 proxy service

%prep
%setup -n npd6-1.0.0


%build
#%%configure
make %{?_smp_mflags}

%install
rm -rf %{buildroot}
make install DESTDIR=%{buildroot}
cp %{buildroot}/etc/npd6.conf.sample %{buildroot}/etc/npd6.conf

%clean
rm -rf %{buildroot}


%files
%defattr(-,root,root,-)
%doc 
%config(noreplace) /etc/npd6.conf
/etc/init.d/npd6
/etc/npd6.conf.sample
/usr/local/bin/npd6
/usr/share/man/man5/npd6.conf.5.gz
/usr/share/man/man8/npd6.8.gz

%changelog
