#!/bin/bash

yum install epel-release -y && yum install bind-utils chrony nginx -y
sed -i '38,84d' /etc/nginx/nginx.conf

###########################################################################
## Append the locally hosted packages directory in /etc/nginx/nginx.conf ##
###########################################################################

cat <<EOF>>/etc/nginx/nginx.conf
server {
   listen *:80;
   server_name 127.0.0.1 ${LOCAL_IP};
   location /0 {
   root /var/artifacts;
   autoindex on;
             }
         }
    }
EOF

## Run the following commands to start nginx service

systemctl start nginx;systemctl enable nginx;systemctl start chronyd;systemctl enable chronyd

## Run the following commands to allow HTTP traffic

firewall-cmd --permanent --zone=public --add-service=http
firewall-cmd --reload

## Download script
cp ~/cortx/cortx-prvsnr/srv/components/provisioner/scripts/install.sh $SCRIPT_PATH
chmod +x ${SCRIPT_PATH}/*.sh
