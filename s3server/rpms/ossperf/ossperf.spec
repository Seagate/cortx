Name:		ossperf
Version:	3.0
Release:	1
Summary:	ossperf tool for test

License:	GPL
URL:	        https://github.com/christianbaun/ossperf
Source:	        %{name}-%{version}.tar.gz
Patch:          ossperf.patch
Requires:	s3cmd >= 1.6.1
Requires:	parallel
Requires:       bc


%description -n ossperf
This script analyzes the performance and data integrity of
S3-compatible storage services

%prep
%setup -q
%patch -p1

%install
rm -rf %{buildroot}

install -d $RPM_BUILD_ROOT%{_bindir}/

cp  ossperf.sh $RPM_BUILD_ROOT%{_bindir}/

%clean
rm -rf %{buildroot}

%files -n ossperf
%license LICENSE
%doc README.md
%{_bindir}/ossperf.sh
