#!/bin/sh

ldapdelete -x -w seagate -r "ak=AKIAJPINPFRBTPAYXAHZ,ou=accesskeys,dc=s3,dc=seagate,dc=com" -D "cn=admin,dc=seagate,dc=com"

ldapdelete -x -w seagate -r "o=s3-background-delete-svc,ou=accounts,dc=s3,dc=seagate,dc=com" -D "cn=admin,dc=seagate,dc=com"

