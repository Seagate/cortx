#!/bin/sh -e

#Script to build Jclient & Jcloud client.

SRC_ROOT="$(dirname "$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd ) " )"
CWD=$(pwd)
USAGE="USAGE: bash $(basename "$0") [clean|package] [--help | -h]

where:
clean              clean previous build
package            build and package
--help             display this help and exit"

if [ $# -ne 1 ]
   then
     echo "Invalid arguments passed"
     echo "$USAGE"
     exit 1
fi

case "$1" in
    clean )
       cd $SRC_ROOT/auth-utils/jclient
       mvn clean
       cd $SRC_ROOT/auth-utils/jcloudclient
       mvn clean
       cd $CWD
        ;;
    package )
       cd $SRC_ROOT/auth-utils/jclient
       mvn package
       cd $SRC_ROOT/auth-utils/jcloudclient
       mvn package
       cd $CWD
        ;;
    --help | -h )
        echo "$USAGE"
        exit 1
        ;;
    * )
        echo "Invalid argument passed"
        echo "$USAGE"
        exit 1
        ;;
esac
