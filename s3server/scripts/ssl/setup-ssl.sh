#!/bin/bash

#####################################
# Self-signed Certificate Generator #
#####################################

set -e

USAGE="USAGE: bash $(basename "$0") [--help]
                   { --seagate | --aws | --san-file File }
                   { --cert-name OutputFileName }
Generate self-signed certificates using OpenSSL.

where:
--seagate       Generate certificate for Seagate domain names
--aws           Generate certificate for Amazon AWS domain names
--san-file      Generate certificate for domain names provided in file
--cert-name     Name of certificate file to be generated
--help          display this help and exit"

CN=''
SAN=''
CURRENT_DIR=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )
SSL_CNF_FILE="$CURRENT_DIR/openssl.cnf"
COUNTRY='IN'
LOCALITY='Pune'
ORG='Seagate Tech'

case "$1" in
    --seagate )
        CN='s3.seagate.com'
        SAN='DNS:s3.seagate.com, DNS:*.s3.seagate.com'
        ;;
    --aws )
        CN='s3.amazonaws.com'
        SAN="DNS: s3.amazonaws.com, DNS:*.s3.amazonaws.com, DNS:s3-us-west-2.amazonaws.com,\
            DNS:*.s3-us-west-2.amazonaws.com"
        ;;
    --san-file )
        [[ -z $2 ]] && echo 'ERROR: File name missing.' && exit 1
        file=$2
        CN=$(head -n 1 $file)
        SAN=$(awk -v ORS=', ' '{print "DNS:"$1}' $file)
        SAN=${SAN:0:-2}
        ;;
    --help | * )
        echo "$USAGE"
        exit 1
        ;;
esac

[[ -z $CN ]] || [[ -z $SAN ]] && echo 'Please specify CN and SAN values.' && exit

if [[ "$3" == "--cert-name" ]]; then
    [[ -z $4 ]] && echo 'ERROR: Specify certificate name to be created' && exit 1
        cert_name=$4
        echo "Using [$4] as certificate file name"
else
    echo "Using common name(CN)[$CN] as certificate file name"
    cert_name=$CN
    echo $cert_name
fi


export SAN=$SAN

echo -e "Generating self-signed certificates for CN = $CN\nWith SANs =" $SAN

rm -rf "$CURRENT_DIR/ssl_sandbox"
mkdir -p "$CURRENT_DIR/ssl_sandbox"
cd "$CURRENT_DIR/ssl_sandbox"

openssl req -new -x509 -extensions v3_ca  -keyout ca.key -out ca.crt -days 3650\
    -nodes -subj "/C=$COUNTRY/L=$LOCALITY/O=$ORG/CN=$CN" 2>/dev/null

openssl genrsa -out "$cert_name.key" 2048 2>/dev/null

openssl req -new -key  "$cert_name.key" -out "$cert_name.csr" -config $SSL_CNF_FILE\
    -subj "/C=$COUNTRY/L=$LOCALITY/O=$ORG/CN=$CN"

openssl x509 -req -days 365 -in "$cert_name.csr" -CA ca.crt -CAkey ca.key -set_serial 01\
    -out "$cert_name.crt" -extfile $SSL_CNF_FILE -extensions v3_ca 2>/dev/null

echo "ssl_certificate: $CURRENT_DIR/ssl_sandbox/$cert_name.crt"
echo "ssl_certificate_key: $CURRENT_DIR/ssl_sandbox/$cert_name.key"
