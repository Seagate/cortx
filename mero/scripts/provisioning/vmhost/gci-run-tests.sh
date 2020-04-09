#!/usr/bin/env bash
# set -eu -o pipefail ## commands tend to fail unpredictably

print_msg()
{
    echo -e "\n[${FUNCNAME[1]}] $1"
}


print_msg "RUN mero system test cases"
cd /data/mero
sudo ./scripts/m0 run-st
RESULT=$?
if [ $RESULT != 0 ]; then
    print_msg "ERROR [sudo ./scripts/m0 run-st] failed!!! Returned $RESULT;"
else
    print_msg "Mero ST was run successfully."
fi

exit $RESULT


