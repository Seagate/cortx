#!/bin/sh
set -e
read -p "Enter the label for S3 setup:" s3_setup_label
read -p "S3 endpoint url:" s3_url_endpoint
read -p "Access Key:" s3_access_key
read -p "Secret Key:" s3_secret_key
s3_setup_label="$s3_setup_label.properties"
echo "s3_endpoint:$s3_url_endpoint" > "$s3_setup_label"
echo "access_key:$s3_access_key" >> "$s3_setup_label"
echo "secret_key:$s3_secret_key" >> "$s3_setup_label"
printf "\nCreated S3 setup properties file $s3_setup_label\n\n"
