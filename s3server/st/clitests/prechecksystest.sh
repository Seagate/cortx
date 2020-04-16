#!/bin/sh

set -e
abort()
{
    echo >&2 '
***************
*** ABORTED ***
***************
'
    echo "Error encountered. Precheck failed..." >&2
    trap : 0
    exit 1
}
trap 'abort' 0

PY_EXPECTED_V=3.4.8  # at least 3.4.8 or above
PY_VER=`python3.6 -c "import platform;print(platform.python_version())";`

function is_x_gt_eq_to_y() {
   x=$1
   y=$2
   test "$(echo "$@" | tr " " "\n" | sort -rV | head -n 1)" = "$x";
}
if !(is_x_gt_eq_to_y $PY_VER $PY_EXPECTED_V); then
   echo "Python version should be atleast greater than $PY_EXPECTED_V"
fi

JCLIENTJAR='jclient.jar'
JCLOUDJAR='jcloudclient.jar'

# Installed s3cmd should have support for --max-retries
# This support is available in patched s3cmd rpm built by our team
# See <s3 server source>/rpms/s3cmd/buildrpm.sh
s3cmd --help | grep max-retries >/dev/null 2>&1
if [ "$?" == "0"  ] ;then
    printf "\nCheck S3CMD...OK"
else
    printf "\nInstalled s3cmd version does not support --max-retries."
    printf "\nPlease install patched version built from <s3server src>/rpms/s3cmd/"
    abort
fi

# Check s3iamcli is installed
if command -v s3iamcli >/dev/null 2>&1; then
    printf "\nCheck s3iamcli...OK"
else
    printf "\ns3iamcli not installed"
    printf "\nPlease install s3iamcli using rpm built from <s3server repo>/rpms/s3iamcli/"
    abort
fi

if [ -f $JCLIENTJAR ] ;then
    printf "\nCheck $JCLIENTJAR...OK"
else
    printf "\nCheck $JCLIENTJAR...Not found"
    abort
fi

if [ -f $JCLOUDJAR ] ;then
    printf "\nCheck $JCLOUDJAR...OK"
else
    printf "\nCheck $JCLOUDJAR...Not found"
    abort
fi

# Check parallel is installed
if command -v parallel >/dev/null 2>&1; then
    printf "\nCheck parallel...OK"
else
    printf "\nparallel not installed, needed by ossperf tool"
    abort
fi

# Check bc tool is installed
if command -v bc >/dev/null 2>&1; then
    printf "\Check bc...OK"
else
    printf "\nbc not installed, its needed by ossperf tool"
    abort
fi

# Check md5sum is installed
if command -v md5sum >/dev/null 2>&1; then
    printf "\nCheck md5sum...OK"
else
    printf "\nmd5sum not installed"
    abort
fi

printf "\nCheck seagate host entries for system test..."
getent hosts seagatebucket.s3.seagate.com seagate-bucket.s3.seagate.com >/dev/null
getent hosts seagatebucket123.s3.seagate.com seagate.bucket.s3.seagate.com >/dev/null
getent hosts iam.seagate.com sts.seagate.com >/dev/null
printf "OK \n"

trap : 0
