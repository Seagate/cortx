#!/bin/sh

ldapdelete -x -w seagate -r "ak=AKIAJPINPFRBTPAYOGNA,ou=accesskeys,dc=s3,dc=seagate,dc=com" -D "cn=admin,dc=seagate,dc=com"

ldapdelete -x -w seagate -r "ak=AKIAJTYX36YCKQSAJT7Q,ou=accesskeys,dc=s3,dc=seagate,dc=com" -D "cn=admin,dc=seagate,dc=com"

ldapdelete -x -w seagate -r "o=s3_test,ou=accounts,dc=s3,dc=seagate,dc=com" -D "cn=admin,dc=seagate,dc=com"
