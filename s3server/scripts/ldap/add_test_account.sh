#!/bin/sh

ldapadd -w seagate -x -D "cn=admin,dc=seagate,dc=com" -f test_data.ldif
