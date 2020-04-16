#!/bin/sh
# Script to stop S3 server in dev environment.
# The script will stop all running instances of S3 server.

callgraph_process=0
callgraph_path="/tmp/callgraph.out"
if [ "$1" == "--callgraph" ]
then
    callgraph_process=1
    if ! [[ $2 =~ ^[[:space:]]*$ ]]
    then
        callgraph_path=$2
    fi
fi

MAX_S3_INSTANCES_NUM=20

# s3 port configured in s3config.yaml
s3_port_from_config=`python -c '
import yaml;
print yaml.load(open("/opt/seagate/s3/conf/s3config.yaml"))["S3_SERVER_CONFIG"]["S3_SERVER_BIND_PORT"];
' | tr -d '\r\n'`

instance=0
while [[ $instance -lt $MAX_S3_INSTANCES_NUM ]]
do
  s3port=$(($s3_port_from_config + $instance))
  pidfile="/var/run/s3server.$s3port.pid"
  if [[ -r $pidfile ]]; then
    pidstr=$(cat $pidfile)
    if [ "$pidstr" != "" ]; then
      kill -s TERM $pidstr
    fi # $pidstr
  fi # $pidfile

  ((instance++))
done # while
echo "Waiting for S3 to shutdown..."
sleep 10

instance=0
while [[ $instance -lt $MAX_S3_INSTANCES_NUM ]]
do
  s3port=$(($s3_port_from_config + $instance))
  statuss3=$(ps -aef | grep /var/run/s3server.$s3port.pid | grep $s3port)
  pidfile="/var/run/s3server.$s3port.pid"
  if [ "$statuss3" != "" ]; then
    if [[ -r $pidfile ]]; then
      pidstr=$(cat $pidfile)
      kill -9 $pidstr
    fi
  fi

  if [[ -e $pidfile ]]; then
    rm -f $pidfile
  fi

  ((instance++))
done

if [ $callgraph_process -eq 1 ]
then
    callgrind_annotate --inclusive=yes --tree=both "$callgraph_path" > "$callgraph_path".annotated
fi
