#!/bin/bash -e
# Script to update schema and initialize ldap for S3 Authserver.
# CAUTION: This scipt will delete existing S3 user data.

USAGE="USAGE: bash $(basename "$0") [--defaultpasswd] [-h | --help]
Update S3 schema in OpenLDAP.

where:
--defaultpasswd     use default password i.e. 'seagate' for LDAP
--help              display this help and exit"

defaultpasswd=false
case "$1" in
    --defaultpasswd )
        defaultpasswd=true
        ;;
    --help | -h )
        echo "$USAGE"
        exit 1
        ;;
esac

PASSWORD="seagate"
if [[ $defaultpasswd == false ]]
then
    echo -n "Enter Password for LDAP: "
    read -s PASSWORD && [[ -z $PASSWORD ]] && echo 'Password can not be null.' && exit 1
fi

# Delete all the entries from LDAP.
ldapdelete -x -w $PASSWORD -D "cn=admin,dc=seagate,dc=com" -r "dc=seagate,dc=com"

# Stop slapd
systemctl stop slapd

# Delete the schema from LDAP.
rm -f /etc/openldap/slapd.d/cn\=config/cn\=schema/cn\=\{1\}s3user.ldif

# Start slapd
systemctl start slapd

# Add S3 schema
ldapadd -x -w $PASSWORD -D "cn=admin,cn=config" -f cn\=\{1\}s3user.ldif

# Restart slapd to update the changes
systemctl restart slapd

# Initialize ldap
ldapadd -x -w $PASSWORD -D "cn=admin,dc=seagate,dc=com" -f ldap-init.ldif
