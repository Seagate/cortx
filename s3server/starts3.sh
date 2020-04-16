#!/bin/sh -x

# Start the s3server
export PATH=$PATH:/opt/seagate/s3/bin
if [ $# -eq 0 ]; then
  systemctl start s3server
else
  count=1
  while [[ $count -le $1 ]]
  do
    systemctl start s3server@$count
    ((count++))
  done
fi
