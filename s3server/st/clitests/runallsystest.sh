#!/bin/sh

SCRIPT_PATH=$(readlink -f "$0")
BASEDIR=$(dirname "$SCRIPT_PATH")
usage() {
  echo 'Usage: runallsystest.sh { --no_https | --use_ipv6 | --help | -h}'
  echo 'Parameters as below :                                '
  echo '          --no_https     : Use http for system tests'
  echo '          --use_ipv6     : Use use ipv6 for system tests'
  echo '          --help (-h)    : Display help'
}

sed -i 's/no_ssl =.*$/no_ssl = False/g' $BASEDIR/framework.py


if [ $# -gt 0 ]
then
  while [ -n "$1" ]; do
    case "$1" in
      --no_https)
         sed -i 's/no_ssl =.*$/no_ssl = True/g' $BASEDIR/framework.py
         sed -i 's/use_https=.*$/use_https=false/g' $BASEDIR/jclient.properties
         sed -i 's/use_https=.*$/use_https=false/g' $BASEDIR/jcloud.properties
         shift ;;

      --use_ipv6)
         sed -i 's/use_ipv6 =.*$/use_ipv6 = True/g' $BASEDIR/framework.py
         shift ;;

      -h|--help) usage; exit 0 ;;

      *) echo 'Incorrect command. See runallsystest.sh --help for details.' ; exit 1 ;;
    esac
  done
else
  echo 'Executing ST`s over HTTPS'
fi

set -e
abort()
{
git checkout -- $BASEDIR/framework.py

    echo >&2 '
***************
*** ABORTED ***
***************
'
    echo "Error encountered. Exiting ST prematurely..." >&2
    trap : 0
    exit 1
}
trap 'abort' 0

sh ./prechecksystest.sh

#Using Python 3.6 version for Running System Tests
PythonV="python3.6"

echo "`date -u`: Running auth_spec.py..."
$PythonV auth_spec.py

echo "`date -u`: Running awsiam_spec.py..."
$PythonV awsiam_spec.py

echo "`date -u`: Running auth_spec_negative_and_fi.py..."
$PythonV auth_spec_negative_and_fi.py

echo "`date -u`: Running auth_spec_param_validation.py..."
$PythonV auth_spec_param_validation.py

echo "`date -u`: Running auth_spec_signature_calculation.py..."
$PythonV auth_spec_signature_calculation.py

echo "`date -u`: Running s3cmd_spec.py..."
$PythonV s3cmd_spec.py

echo "`date -u`: Running jclient_spec.py..."
$PythonV jclient_spec.py

echo "`date -u`: Running jcloud_spec.py..."
$PythonV jcloud_spec.py

echo "`date -u`: Running rollback_spec.py..."
$PythonV rollback_spec.py

echo "`date -u`: Running negative_spec.py..."
$PythonV negative_spec.py

echo "`date -u`: Running shutdown_spec.py..."
$PythonV shutdown_spec.py

echo "`date -u`: Running awss3api_spec.py..."
$PythonV awss3api_spec.py

echo "`date -u`: Running aclvalidation_spec.py..."
$PythonV aclvalidation_spec.py

echo "`date -u`: Running policy_spec.py..."
$PythonV policy_spec.py

echo "`date -u`: Running authpassencryptcli_spec.py..."
$PythonV authpassencryptcli_spec.py

git checkout -- $BASEDIR/framework.py

echo >&2 '
**************************
*** ST Runs Successful ***
**************************
'
trap : 0
