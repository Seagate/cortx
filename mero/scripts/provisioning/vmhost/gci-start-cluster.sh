#!/usr/bin/env bash
# set -eu -o pipefail ## commands tend to fail unpredictably

SINGLENODE="/opt/seagate/eos/hare/share/cfgen/examples/singlenode.yaml"

sudo /usr/sbin/m0setup
if [ "$?" != 0 ]; then
    echo -e "\n\n\nERROR !! [/usr/sbin/m0setup] failed.\n\n\n"
    echo -e "\nPress CTRL+C to terminate in 10 seconds."; read -t 10 a
else
    sleep 10
fi

hctl status
if [ "$?" != 0 ]; then
    echo -e "\n\n\nERROR !! [hctl status] failed.\n\n\n"
    echo -e "\nPress CTRL+C to terminate in 10 seconds."; read -t 10 a
fi


hctl bootstrap --mkfs $SINGLENODE
if [ "$?" != 0 ]; then
    echo -e "\n\n\nERROR !! [hctl bootstrap] failed.\n\n\n"
    echo -e "\nPress CTRL+C to terminate in 10 seconds."; read -t 10 a
else
    sleep 30
fi

hctl status
if [ "$?" != 0 ]; then
    echo -e "\n\n\nERROR !! [hctl status] failed.\n\n\n"
    echo -e "\nPress CTRL+C to terminate in 10 seconds."; read -t 10 a
else
    sleep 10
fi

hctl shutdown
if [ "$?" != 0 ]; then
    echo -e "\n\n\nERROR !! [hctl status] failed.\n\n\n"
    echo -e "\nPress CTRL+C to terminate in 10 seconds."; read -t 10 a
else
    sleep 10
fi

sudo m0setup -C
if [ "$?" != 0 ]; then
    echo -e "\n\n\nERROR !! [sudo m0setup -C] failed.\n\n\n"
fi
