Name:           haproxy-statsd
Version:        1.0
Release:        1
Summary:        tool to send haproxy statistics to statsd

License:        MIT
URL:            https://github.com/softlayer/haproxy-statsd
Source:         %{name}-%{version}.tar.gz
Patch:          haproxy-statsd.patch
Requires:       haproxy, python-psutil

%description -n haproxy-statsd
This script sends statistics from haproxy to statsd server.

%prep
%setup -q
%patch -p1

%install
rm -rf %{buildroot}
mkdir -p %{buildroot}/opt/seagate/s3-haproxy-statsd/
cp haproxy-statsd.* %{buildroot}/opt/seagate/s3-haproxy-statsd/
cp LICENSE  %{buildroot}/opt/seagate/s3-haproxy-statsd/
cp README.md  %{buildroot}/opt/seagate/s3-haproxy-statsd/

%clean
rm -rf %{buildroot}

%files
%defattr(-,root,root,-)
%dir /opt/seagate/s3-haproxy-statsd/
%attr(0755, root, root) /opt/seagate/s3-haproxy-statsd/haproxy-statsd.py
/opt/seagate/s3-haproxy-statsd/haproxy-statsd.conf
/opt/seagate/s3-haproxy-statsd/LICENSE
/opt/seagate/s3-haproxy-statsd/README.md

%exclude /opt/seagate/s3-haproxy-statsd/*.pyc
%exclude /opt/seagate/s3-haproxy-statsd/*.pyo

%license LICENSE
%doc README.md
