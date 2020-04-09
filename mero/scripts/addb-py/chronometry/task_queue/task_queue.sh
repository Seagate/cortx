#!/bin/bash

set +x

USER_NAME=root
SERVER_NAME=skln3-m04.mero.colo.seagate.com

ssh $USER_NAME@$SERVER_NAME -T "cd /root/alexander/mero/scripts/addb-py/chronometry/task_queue && python3 task_queue.py $@"
