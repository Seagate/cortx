Name:		statsd-zabbix-backend
Version:	0.2.0
Release:	1
Summary:	statsd-zabbix-backend source
License:	GPL
Source0:	v0.2.0.tar.gz

%description
Makes RPM for statsd-zabbix-backend

%prep
%setup -q

%install
rm -rf "$RPM_BUILD_ROOT"
mkdir -p "$RPM_BUILD_ROOT"/usr/lib/node_modules/statsd-zabbix-backend/
cp -R * "$RPM_BUILD_ROOT"/usr/lib/node_modules/statsd-zabbix-backend/

%files
%defattr(-,root,root,-)
/usr/lib/node_modules/statsd-zabbix-backend/
/usr/lib/node_modules/statsd-zabbix-backend/README.md
/usr/lib/node_modules/statsd-zabbix-backend/package.json
/usr/lib/node_modules/statsd-zabbix-backend/lib
/usr/lib/node_modules/statsd-zabbix-backend/lib/zabbix.js
/usr/lib/node_modules/statsd-zabbix-backend/test
/usr/lib/node_modules/statsd-zabbix-backend/test/test-zabbix.js

%clean
rm -rf "$RPM_BUILD_ROOT"
