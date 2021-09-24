#!/bin/bash

export SCRIPT_PATH=/mnt/cortx/scripts
yum install epel-release -y
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

#######################################################
## Run the following commands to start nginx service ##
#######################################################

systemctl start nginx
systemctl enable nginx

######################################################
## Run the following commands to allow HTTP traffic ##
######################################################

firewall-cmd --permanent --zone=public --add-service=http
firewall-cmd --reload

################################
## Run the following commands ##
################################

mv /var/artifacts/0/install-2.0.0-0.sh $SCRIPT_PATH/install.sh
cd $SCRIPT_PATH && curl -O https://raw.githubusercontent.com/Seagate/cortx-prvsnr/main/srv/components/provisioner/scripts/install.sh
