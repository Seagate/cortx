#!/bin/bash -e
# This script deploys SSL certs and enables SSL in OpenLDAP
# performs below steps
# 1. Copy certificates and key to /etc/ssl/openldap
# 2. Updates OpenLDAP config with certificate locations
# 3. Updates URL's in /etc/sysconfig/slapd to enable ldaps port 636
# 4. Restarts slapd service

USAGE="USAGE: bash $(basename "$0") [-cafile <cacert>] [-certfile <cert>]
              [-keyfile <key>] [-help | -h]
Deploy SSL certs and enable SSL in Openldap
where:
-cafile <cacert>  CA Certificate absolute file path
-certfile <cert>  Certificate absolute file path
-keyfile <key>    Key absolute file path
-help             Display usage and exit"
#Ex :
# ./enable_ssl_openldap.sh -cafile /etc/ssl/stx-s3/openldap/ca.crt \
#                          -certfile /etc/ssl/stx-s3/openldap/s3openldap.crt \
#                          -keyfile /etc/ssl/stx-s3/openldap/s3openldap.key

if [ -z $1 ]
  then
    echo "$USAGE"
    exit 1
fi
numargs=$#

while [ "$1" != "" ]; do
    case $1 in
        -cafile  )      shift
                        cafile=$1
                        ;;
        -certfile )     shift
                        certfile=$1
                        ;;
        -keyfile )      shift
                        keyfile=$1
                        ;;
        -help | -h )   echo "$USAGE"
                        exit
                        ;;
        * )             echo "$USAGE"
                        exit 1
    esac
    shift
done

if [ ! -e $cafile ]
  then
    echo "CA File not found : $cafile"
    exit 1
fi
if [ ! -e $certfile ]
  then
    echo "Cert File not found : $certfile"
    exit 1
fi
if [ ! -e $keyfile ]
  then
    echo "Key File not found : $keyfile"
    exit 1
fi

#echo $cafile
#echo $certfile
#echo $keyfile

ssl_ldif="dn: cn=config
replace: olcTLSCACertificateFile
olcTLSCACertificateFile: $cafile
-
replace: olctlscertificatefile
olctlscertificatefile: $certfile
-
replace: olctlscertificatekeyfile
olctlscertificatekeyfile: $keyfile"

echo "$ssl_ldif" > ssl_certs.ldif

echo "Updating Openldap config.."
ldapmodify -Y EXTERNAL  -H ldapi:/// -f ssl_certs.ldif

rm ssl_certs.ldif

ldap_url="\"ldap:\/\/\/ ldaps:\/\/\/ ldapi:\/\/\/\""
# Backup original slapd file
cp /etc/sysconfig/slapd /etc/sysconfig/slapd_$(date "+%Y.%m.%d-%H.%M.%S")_bak

#echo $ldap_url
sed -i "s/^SLAPD_URLS=.*/SLAPD_URLS=${ldap_url}/" /etc/sysconfig/slapd

echo "Restaring slapd..."
systemctl restart slapd
