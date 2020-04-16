#!/bin/sh -e

#Script to build all Auth server modules

SRC_ROOT="$(dirname "$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd ) " )"

USAGE="USAGE: bash $(basename "$0") [clean|package|jacoco-report] [--help | -h]

where:
clean              clean previous build
package            build and package
jacoco-report      generate system test coverage report
--help             display this help and exit"

if [ -z $1 ]
   then
     echo "$USAGE"
     exit 1
fi
defaultpasswd=false
case "$1" in
    clean )
       cd $SRC_ROOT/auth/encryptutil
       mvn clean
       cd $SRC_ROOT/auth/encryptcli
       mvn clean
       cd $SRC_ROOT/auth/server
       mvn clean
        ;;
    package )
       cd $SRC_ROOT/auth/encryptutil
       mvn package
       mvn install:install-file -Dfile=target/AuthEncryptUtil-1.0-0.jar -DgroupId=com.seagates3 -DartifactId=AuthEncryptUtil -Dversion=1.0-0 -Dpackaging=jar
       cd $SRC_ROOT/auth/encryptcli
       mvn package
       cd $SRC_ROOT/auth/server
       mvn package
        ;;
    jacoco-report )
       cd $SRC_ROOT/auth/encryptutil
       mvn jacoco:report
       cd $SRC_ROOT/auth/encryptcli
       mvn jacoco:report
       cd $SRC_ROOT/auth/server
       mvn jacoco:report
        ;;
    --help | -h )
        echo "$USAGE"
        exit 1
        ;;
esac
