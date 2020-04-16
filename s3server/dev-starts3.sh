#!/bin/sh -e
# Script to start S3 server in dev environment.
#   Usage: sudo ./dev-starts3.sh [<Number of S3 sever instances>] [--fake_obj] [--fake_kvs | --redis_kvs] [--callgraph /path/to/graph]
#               Optional argument is:
#                   Number of S3 server instances to start.
#                   Max number of instances allowed = 20
#                   Default number of instances = 1

MAX_S3_INSTANCES_NUM=20

set +e
tmp=$(systemctl status rsyslog)
res=$?
if [ "$res" -eq "4" ]
then
    echo "Rsyslog service not found. Exiting..."
    exit -1
fi
if [ "$res" -ne "0" ]
then
    echo "Starting Rsyslog..."
    tmp=$(systemctl start rsyslog)
    res=$?
    if [ "$res" -ne "0" ]
    then
        echo "Rsyslog service start failed. Exiting..."
        exit -1
    fi
fi
echo "rsyslog started"
set -e

num_instances=1
fake_obj=0
fake_kvs=0
redis_kvs=0

callgraph_mode=0
callgraph_out="/tmp/callgraph.out"

if [[ $1 =~ ^[0-9]+$ ]] && [ $1 -le $MAX_S3_INSTANCES_NUM ]
then
    num_instances=$1
    shift
fi

while [ "$1" != "" ]; do
    case "$1" in
        --fake_obj ) fake_obj=1;
                     echo "Stubs for clovis object read/write ops";
                     ;;
        --fake_kvs ) fake_kvs=1;
                     echo "Stubs for clovis kvs put/get/delete/create idx/remove idx";
                     ;;
        --redis_kvs ) redis_kvs=1;
                      echo "Redis based stubs for clovis kvs put/get/delete";
                      ;;
        --callgraph ) callgraph_mode=1;
                      num_instances=1;
                      echo "Generate call graph with valgrind";
                      shift;
                      if ! [[ $1 =~ ^[[:space:]]*$ ]]
                      then
                          callgraph_out="$1";
                      fi
                      ;;
        * )
            echo "Invalid argument passed";
            exit 1
            ;;
    esac
    shift
done

if [ $fake_kvs == 1 ] && [ $redis_kvs == 1 ]; then
    echo "Only fake kvs or redis kvs can be specified";
    exit 1;
fi

set -x

export LD_LIBRARY_PATH="$(pwd)/third_party/mero/mero/.libs/:"\
"$(pwd)/third_party/mero/helpers/.libs/:"\
"$(pwd)/third_party/mero/extra-libs/gf-complete/src/.libs/"

# Get local address
modprobe lnet
lctl network up &>> /dev/null
local_nid=`lctl list_nids | head -1`
local_ep=$local_nid:12345:33
ha_ep=$local_nid:12345:34:1

#Set the core file size to unlimited
ulimit -c unlimited

#Set max open file limit to 10240
ulimit -n 10240

# Run m0dixinit
set +e
./third_party/mero/dix/utils/m0dixinit -l $local_nid:12345:34:100 -H $local_nid:12345:34:1 \
                 -p '<0x7000000000000001:0>' -I 'v|1:20' -d 'v|1:20' -a check 2> /dev/null \
                 | grep -E 'Metadata exists: false' > /dev/null
rc=$?
set -e
if [ $rc -eq 0 ]
then
./third_party/mero/dix/utils/m0dixinit -l $local_nid:12345:34:100 -H $local_nid:12345:34:1 \
                 -p '<0x7000000000000001:0>' -I 'v|1:20' -d 'v|1:20' -a create
fi

# Ensure default working dir is present
s3_working_dir=`python -c '
import yaml;
print yaml.load(open("/opt/seagate/s3/conf/s3config.yaml"))["S3_SERVER_CONFIG"]["S3_DAEMON_WORKING_DIR"];
' | tr -d '\r\n'
`"/s3server-0x7200000000000000:0"

mkdir -p $s3_working_dir

# Log dir configured in s3config.yaml
s3_log_dir=`python -c '
import yaml;
print yaml.load(open("/opt/seagate/s3/conf/s3config.yaml"))["S3_SERVER_CONFIG"]["S3_LOG_DIR"];
' | tr -d '\r\n'
`"/s3server-0x7200000000000000:0"
mkdir -p $s3_log_dir

# s3 port configured in s3config.yaml
s3_port_from_config=`python -c '
import yaml;
print yaml.load(open("/opt/seagate/s3/conf/s3config.yaml"))["S3_SERVER_CONFIG"]["S3_SERVER_BIND_PORT"];
' | tr -d '\r\n'`

# Start the s3server
export PATH=$PATH:/opt/seagate/s3/bin
counter=0

# s3server cmd parameters allowing to fake some clovis functionality
# --fake_clovis_writeobj - stub for clovis write object with all zeros
# --fake_clovis_readobj - stub for clovis read object with all zeros
# --fake_clovis_createidx - stub for clovis create idx - does nothing
# --fake_clovis_deleteidx - stub for clovis delete idx - does nothing
# --fake_clovis_getkv - stub for clovis get key-value - read from memory hash map
# --fake_clovis_putkv - stub for clovis put kye-value - stores in memory hash map
# --fake_clovis_deletekv - stub for clovis delete key-value - deletes from memory hash map
# for proper KV mocking one should use following combination
#    --fake_clovis_createidx true --fake_clovis_deleteidx true --fake_clovis_getkv true --fake_clovis_putkv true --fake_clovis_deletekv true

fake_params=""
if [ $fake_kvs -eq 1 ]
then
    fake_params+=" --fake_clovis_createidx true --fake_clovis_deleteidx true --fake_clovis_getkv true --fake_clovis_putkv true --fake_clovis_deletekv true"
fi

if [ $redis_kvs -eq 1 ]
then
    fake_params+=" --fake_clovis_createidx true --fake_clovis_deleteidx true --fake_clovis_redis_kvs true"
fi

if [ $fake_obj -eq 1 ]
then
    fake_params+=" --fake_clovis_writeobj true --fake_clovis_readobj true"
fi

callgraph_cmd=""
if [ $callgraph_mode -eq 1 ]
then
    callgraph_cmd="valgrind -q --tool=callgrind --collect-jumps=yes --collect-systime=yes --callgrind-out-file=$callgraph_out"
fi

while [[ $counter -lt $num_instances ]]
do
  clovis_local_port=`expr 101 + $counter`
  s3port=`expr $s3_port_from_config + $counter`
  pid_filename='/var/run/s3server.'$s3port'.pid'
  $callgraph_cmd s3server --s3pidfile $pid_filename \
           --clovislocal $local_ep:${clovis_local_port} --clovisha $ha_ep \
           --s3port $s3port --fault_injection true $fake_params --loading_indicators --addb
  ((++counter))
done
