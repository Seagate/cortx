#!/bin/sh

# This scripts creates RPM from statsd-zabbix-backend source
# using the SPEC file.
set -e

CURRENT_DIR=`pwd`
rm -rf ~/rpmbuild
rpmdev-setuptree
wget https://github.com/parkerd/statsd-zabbix-backend/archive/v0.2.0.tar.gz
mv v0.2.0.tar.gz ~/rpmbuild/SOURCES/
cp statsd_zabbix_backend.spec ~/rpmbuild/SPECS/
rpmbuild -ba ~/rpmbuild/SPECS/statsd_zabbix_backend.spec
cd "$CURRENT_DIR"
echo "RPM created successfully in ~/rpmbuild/RPMS dir"
