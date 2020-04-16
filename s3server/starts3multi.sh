#!/bin/sh -x

# Start the s3server
export PATH=$PATH:/opt/seagate/s3/bin

# Get local address
modprobe lnet
lctl network up &>> /dev/null
LOCAL_NID=`lctl list_nids | head -1`
LOCAL_EP=$LOCAL_NID:12345:44
HA_EP=$LOCAL_NID:12345:45:1
CONFD_EP=$LOCAL_NID:12345:44:101
PROF_OPT='<0x7000000000000001:0>'

clovis_local_port=100

for i in {8080..8099}
do
  cmd="s3server --clovislocal $LOCAL_EP:${clovis_local_port} --clovisha $HA_EP --clovisconfd $CONFD_EP --s3port ${i} --authhost 127.0.0.1 --authport 9085 --log_dir /var/log/seagate/s3 --s3loglevel INFO"
  echo $cmd
  eval $cmd &
  ((clovis_local_port++))
done
