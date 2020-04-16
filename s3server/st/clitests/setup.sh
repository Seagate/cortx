#!/bin/sh

# This script installs virtual env and creates a virtual env for python3.5
# in the user's home directory (/root if this script is run as root user.)
#
# It also installs all the dependencies required for system tests.

pip install virtualenv
virtualenv -p /usr/local/bin/python3.5 mero_st
source mero_st/bin/activate
pip install python-dateutil==2.4.2
pip install pyyaml==3.11
pip install xmltodict==0.9.2
pip install boto3==1.2.2
pip install botocore==1.3.8
pip install scripttest==1.3

echo ""
echo "Add the following to /etc/hosts"
echo "127.0.0.1 seagatebucket.s3.seagate.com seagate-bucket.s3.seagate.com seagatebucket123.s3.seagate.com seagate.bucket.s3.seagate.com"
echo "127.0.0.1 s3-us-west-2.seagate.com seagatebucket.s3-us-west-2.seagate.com"
echo "127.0.0.1 iam.seagate.com sts.seagate.com"
