#!/bin/bash

SCRIPT_PATH=/mnt/cortx/scripts
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
sed -i '/udx-discovery/d;/uds-pyi/d' $SCRIPT_PATH/install.sh
sed -i 's/trusted-host: cortx-storage.colo.seagate.com/trusted-host: '$LOCAL_IP'/' $SCRIPT_PATH/install.sh
sed -i 's#cortx-storage.colo.seagate.com|file://#cortx-storage.colo.seagate.com|baseurl=file:///#' $SCRIPT_PATH/install.sh
sed -i '269s#yum-config-manager --add-repo "${repo}/3rd_party/" >> "${LOG_FILE}"#yum-config-manager --nogpgcheck --add-repo "${repo}/3rd_party/" >> "${LOG_FILE}"#' $SCRIPT_PATH/install.sh
