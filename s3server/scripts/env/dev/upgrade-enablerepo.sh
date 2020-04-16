#!/bin/sh
set -x
yum upgrade
subscription-manager repos --enable=codeready-builder-for-rhel-8-x86_64-rpms
subscription-manager repos --enable=ansible-2-for-rhel-8-x86_64-rpms
subscription-manager repos --enable=rhel-8-for-x86_64-highavailability-rpms
