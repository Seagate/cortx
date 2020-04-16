#!/bin/sh

ldapadd -w seagate -x -D "cn=admin,dc=seagate,dc=com" -f /opt/seagate/s3/install/ldap/background_delete_account.ldif
