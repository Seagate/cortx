#!/bin/sh
# Script to update hostname entries in /etc/hosts

set -e

USAGE="USAGE: bash $(basename "$0") [--help]
                   { --host HostIpv4 }
Update host entries in /etc/hosts

where:
--host        Use the provided ipv4 address for host \"$HOSTNAME\"
--help (-h)   Display help"

loopback_ip="127.0.0.1"
file="/etc/hosts"
s3_host_entries="seagatebucket.s3.seagate.com seagate-bucket.s3.seagate.com
                 seagatebucket123.s3.seagate.com seagate.bucket.s3.seagate.com
                 s3-us-west-2.seagate.com seagatebucket.s3-us-west-2.seagate.com
                 iam.seagate.com sts.seagate.com s3.seagate.com"
defaulthost=false
s3_host_default="$(hostname -I | cut -d' ' -f1)"
s3_host_ip=""

case "$1" in
    --host )
        echo "Using $2 as ipv4 address for \"$HOSTNAME\" "
        s3_host_ip=$2
        defaulthost=true
        ;;
    --help | -h )
        echo "$USAGE"
        exit 0
        ;;
esac

if [[ $defaulthost == false ]]
then
  read -p "Enter ipv4 address for host \"$HOSTNAME\" (default is $s3_host_default): " s3_host_ip
  [ "$s3_host_ip" == "" ] && s3_host_ip=$s3_host_default
fi

#Check whether given host is reachable.
if ! ping -c1 $s3_host_ip &> /dev/null
then
  echo "Host $s3_host_ip is unreachable"
  exit 1
else
  echo "Host $s3_host_ip is reachable"
fi


#Check whether multiple localhost entries are present or hostname is already configured.
loopback_ip_count=$(grep "^$loopback_ip" /etc/hosts|wc -l)
if [ $loopback_ip_count -ne 1 ]
then
  echo "\"$loopback_ip\" is used $loopback_ip_count times"
  exit 1
fi

s3_host=$(grep "$HOSTNAME" /etc/hosts|wc -l)
if [ $s3_host -ne 0 ]
then
  echo "\"$HOSTNAME\" is already configured, cannot be added again to /etc/hosts"
fi

#Backup original /etc/hosts file
cp -f /etc/hosts /etc/hosts.backup.`date +"%Y%m%d_%H%M%S"`

#Checking for duplication & adding s3 host entry
for host_entry in $s3_host_entries
do
  # search_entry is for escaping '.'
  search_entry="$(echo $host_entry | sed 's/\./\\./g')"
  found=$(egrep "(^|\s+)$search_entry(\s+|$)" $file |wc -l)
  if [ $found -eq 0 ]
  then
    sed -i "/^$loopback_ip/s/$/ $search_entry/" $file
  else
    echo "\"$host_entry\" is already configured, cannot be added again to /etc/hosts"
    exit 0
  fi
done

#To add s3 hostname entry to /etc/hosts
if [ $s3_host_ip = $loopback_ip ]
then
  # Append s3 hostname to end of loopback_ip
  sed -i "/^$loopback_ip/s/$/ $HOSTNAME/" $file
else
  # Append s3 hostname to end of /etc/hosts
  s3_host_entry="$s3_host_ip   $HOSTNAME"
  echo "$s3_host_entry" >> $file
fi
