#!/bin/sh

# This scripts creates RPM from statsd-zabbix-backend source using FPM

set -e
RPM_SANDBOX_DIR=rpmssandbox
RPM_PKG_NAME=statsd-zabbix-backend
RPM_VERSION=0.2.0

wget https://github.com/parkerd/statsd-zabbix-backend/archive/v0.2.0.tar.gz
tar xvzf v0.2.0.tar.gz
mkdir -p $RPM_SANDBOX_DIR/usr/lib/node_modules/$RPM_PKG_NAME/
cp -r statsd-zabbix-backend-0.2.0/* $RPM_SANDBOX_DIR/usr/lib/node_modules/$RPM_PKG_NAME/

fpm -s dir -t rpm -C $RPM_SANDBOX_DIR \
    --name $RPM_PKG_NAME \
    --version $RPM_VERSION \
    --directories /usr/lib/node_modules/$RPM_PKG_NAME \
    --description "StatsD Zabbix Backend"
