#!/bin/bash
set -e

USAGE() { echo "USAGE: bash $(basename "$0") [-h]
                   [-f domain_input.conf][-d][-m]
Configurations for generating S3 certificates.
Optional params as below:
    -f : Use configuration from the specified config file
    -d : Use default configurations
    -m : Provide manual entries for configurations
    -h : Display help" ; exit 1; }


SCRIPT_PATH=$(readlink -f "$0")
BASEDIR=$(dirname "$SCRIPT_PATH")
VALID_COMMAND_LINE_OPTIONS="f:mdh"

[ $# -eq 0 ] && USAGE

while getopts $VALID_COMMAND_LINE_OPTIONS option; do
  case "${option}" in
    f)
      CONFIG_FILE="$BASEDIR/${OPTARG}"

      if [ -f "$CONFIG_FILE" ]
      then
        echo "Configuration file $CONFIG_FILE found."

      while IFS="=" read -r key value; do
        case "$key" in
          "issuer_name") issuer_name="$value" ;;
          "multi_domain_endpoint") multi_domain_endpoint="$value" ;;
          "s3_default_endpoint") s3_default_endpoint="$value" ;;
          "s3_region_endpoint") s3_region_endpoint="$value" ;;
          "s3_iam_endpoint") s3_iam_endpoint="$value" ;;
          "s3_sts_endpoint") s3_sts_endpoint="$value" ;;
          "s3_ip_address") s3_ip_address="$value" ;;
          "openldap_domainname") openldap_domainname="$value" ;;
          "passphrase") passphrase="$value" ;;
        esac
      done < $CONFIG_FILE
      else
        echo "Configuration file $CONFIG_FILE not found."
        exit 1
      fi
      ;;
    m)
      read -p "Enter certificate issuer name (default is seagate.com): " issuer_name
      read -p "Enter multi domain endpoint (default is *.seagate.com): " multi_domain_endpoint
      read -p "Enter S3 endpoint (default is s3.seagate.com): " s3_default_endpoint
      read -p "Enter S3 region endpoint (default is s3-us-west-2.seagate.com): " s3_region_endpoint
      read -p "Enter S3 iam endpoint (default is iam.seagate.com): " s3_iam_endpoint
      read -p "Enter S3 sts endpoint (default is sts.seagate.com): " s3_sts_endpoint
      read -p "Enter host S3 ip address (for dev vm use 127.0.0.1,::1): " s3_ip_address
      read -p "Enter Open ldap domain name (default is localhost): " openldap_domainname
      read -p "Enter the key store passphrase (default is seagate): " passphrase
      ;;
    d)
      issuer_name="seagate.com"
      multi_domain_endpoint="*.seagate.com"
      s3_default_endpoint="s3.seagate.com"
      s3_region_endpoint="s3-us-west-2.seagate.com"
      s3_iam_endpoint="iam.seagate.com"
      s3_sts_endpoint="sts.seagate.com"
      openldap_domainname="localhost"
      s3_ip_address="127.0.0.1,::1"
      passphrase="seagate"
      ;;
    h|?)
      USAGE
      ;;
    esac
done

echo "Using following configurations
  issuer_name = $issuer_name
  multi_domain_endpoint = $multi_domain_endpoint
  S3 endpoint = $s3_default_endpoint
  S3 region endpoint = $s3_region_endpoint
  S3 iam endpoint = $s3_iam_endpoint
  S3 sts endpoint = $s3_sts_endpoint
  S3 ip address = $s3_ip_address
  Open ldap domain name = $openldap_domainname
  Keystore passpharse = $passphrase"

[ -z $issuer_name ] && { echo "Certificate issuer name is required" ; exit 1; }
[ -z $multi_domain_endpoint ] && { echo "Multi domain endpoint name is required" ; exit 1; }
[ -z $s3_default_endpoint ] && { echo "S3 endpoint is required" ; exit 1; }
[ -z $s3_region_endpoint ] && { echo "S3 region endpoint is required" ; exit 1; }
[ -z $s3_iam_endpoint ] && { echo "S3 iam endpoint is required" ; exit 1; }
[ -z $s3_sts_endpoint ] && { echo "S3 sts endpoint is required" ; exit 1; }
[ -z $openldap_domainname ] && { echo "Open ldap domain name is required" ; exit 1; }
[ -z $passphrase ] && { echo "Key store passpharse is required" ; exit 1; }

# Global file names
s3_auth_cert_file_name="s3authserver"
s3_cert_file_name="s3server"
s3_openldap_cert_file_name="s3openldap"

function generate_s3_certs()
{
  # create dns.list file in ssl folder with given domain names.
  # generate ssl certificate and key
  ssl_sandbox="$CURRENT_DIR/ssl_sandbox"
  if [ -d "$ssl_sandbox" ]
  then
    echo "removing existing directory: $ssl_sandbox"
    rm -rf $ssl_sandbox
  fi

  # create dns list files
  dns_list_file="$CURRENT_DIR/dns.list"
  rm -f $dns_list_file

  # Use mulitdomain name in dns list to use single certificate
  # with *.seagate.com as SAN list
  # Please refer EOS-6038 for more details
  echo $issuer_name > $dns_list_file
  echo $multi_domain_endpoint >> $dns_list_file
  #echo $s3_default_endpoint >> $dns_list_file
  echo "*.$s3_default_endpoint" >> $dns_list_file
  echo "*.$s3_region_endpoint" >> $dns_list_file
  #echo $s3_region_endpoint | tr , '\n' >> $dns_list_file

  if [ ! -z "$s3_ip_address" ]
  then
    echo $s3_ip_address | tr , '\n' >> $dns_list_file
  fi

  # generate s3 ssl cert files
  $CURRENT_DIR/setup-ssl.sh --san-file $dns_list_file \
                            --cert-name $s3_cert_file_name

  cat ${ssl_sandbox}/${s3_cert_file_name}.crt \
      ${ssl_sandbox}/${s3_cert_file_name}.key \
      > ${ssl_sandbox}/${s3_cert_file_name}.pem

  \cp $ssl_sandbox/* $s3_dir

  # cleanup
  rm -f $dns_list_file
  rm -rf $ssl_sandbox
}

function generate_jks_and_iamcert()
{
  #san_region_endpoint=`echo $s3_region_endpoint | sed 's/,/,dns:/g'`
  san_list="dns:$s3_iam_endpoint,dns:$s3_sts_endpoint"

  # Create s3authserver.jks keystore file
  keytool -genkeypair -keyalg RSA -alias s3auth \
          -keystore ${s3auth_dir}/s3authserver.jks -storepass ${passphrase} \
          -keypass ${passphrase} -validity 3600 -keysize 2048 \
          -dname "C=IN, ST=Maharashtra, L=Pune, O=Seagate, OU=S3, CN=$s3_iam_endpoint" \
          -ext SAN=$san_list

  # Steps to generate crt file from Key store
  keytool -importkeystore -srckeystore ${s3auth_dir}/s3authserver.jks \
          -destkeystore ${s3auth_dir}/s3authserver.p12 -srcstoretype jks \
          -deststoretype pkcs12 -srcstorepass ${passphrase} \
          -deststorepass ${passphrase}

  openssl pkcs12 -in ${s3auth_dir}/s3authserver.p12 \
          -out ${s3auth_dir}/s3authserver.jks.pem \
          -passin pass:${passphrase} -passout pass:${passphrase}

  openssl x509 -in ${s3auth_dir}/s3authserver.jks.pem \
          -out ${s3auth_dir}/${s3_auth_cert_file_name}.crt

  # Extract Private Key
  openssl pkcs12 -in ${s3auth_dir}/s3authserver.p12 -nocerts \
          -out ${s3auth_dir}/s3authserver.jks.key -passin pass:${passphrase} \
          -passout pass:${passphrase}

  # Decrypt using pass phrase
  openssl rsa -in ${s3auth_dir}/s3authserver.jks.key \
          -out ${s3auth_dir}/${s3_auth_cert_file_name}.key \
          -passin pass:${passphrase}

  cat ${s3auth_dir}/${s3_auth_cert_file_name}.crt \
      ${s3auth_dir}/${s3_auth_cert_file_name}.key \
      > ${s3auth_dir}/${s3_auth_cert_file_name}.pem

  ## Steps to create Key Pair for password encryption and store it in java keystore
  ## This key pair will be used by AuthPassEncryptCLI and AuthServer for encryption and
  ## decryption of ldap password respectively
  keytool -genkeypair -keyalg RSA -alias s3auth_pass \
          -keystore ${s3auth_dir}/s3authserver.jks -storepass ${passphrase} \
          -keypass ${passphrase} -validity 3600 -keysize 512 \
          -dname "C=IN, ST=Maharashtra, L=Pune, O=Seagate, OU=S3, CN=$s3_iam_endpoint" \
          -ext SAN=$san_list

}

function import_s3_ldap_cert_in_jks() {
  # import open ldap cert into jks
  keytool -import -trustcacerts -keystore ${s3auth_dir}/s3authserver.jks \
          -storepass ${passphrase} -noprompt -alias ldapcert \
          -file ${openldap_dir}/${s3_openldap_cert_file_name}.crt

  # import s3 ssl cert into jks file
  keytool -import -trustcacerts -keystore ${s3auth_dir}/s3authserver.jks \
          -storepass ${passphrase} -noprompt -alias s3 \
          -file ${s3_dir}/${s3_cert_file_name}.crt
}

function generate_openldap_cert()
{
  # create dns.list file in ssl folder with openldap domainname.
  # generate ssl certificate and key
  ssl_sandbox="$CURRENT_DIR/ssl_sandbox"
  if [ -d "$ssl_sandbox" ]
  then
    echo "removing existing directory: $ssl_sandbox"
    rm -rf $ssl_sandbox
  fi

  # create dns list file
  dns_list_file="$CURRENT_DIR/dns.list"
  rm -f $dns_list_file
  echo "$openldap_domainname" > $dns_list_file

  # generate openldap cert files
  $CURRENT_DIR/setup-ssl.sh --san-file $dns_list_file \
                            --cert-name $s3_openldap_cert_file_name

  \cp $ssl_sandbox/* $openldap_dir

  rm -f $dns_list_file
  rm -rf $ssl_sandbox
}

CURRENT_DIR=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )
sand_box=`pwd`"/s3_certs_sandbox"

# create target directories
rm -rf $sand_box
mkdir -p $sand_box
s3_dir="$sand_box/s3"
mkdir -p $s3_dir
s3auth_dir="$sand_box/s3auth"
mkdir -p $s3auth_dir
openldap_dir="$sand_box/openldap"
mkdir -p $openldap_dir

# 1. genearte ssl certificates
generate_s3_certs
# 2. generate openldap cert
generate_openldap_cert
# 3. generate jks
generate_jks_and_iamcert
# 4. import s3 and openldap crt in jks
import_s3_ldap_cert_in_jks


echo
echo "## Certificate generation is complete in [$sand_box] ##"
