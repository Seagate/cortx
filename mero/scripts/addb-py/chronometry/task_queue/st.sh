#!/bin/bash

#set -x
set -e

stop_and_exit()
{
    local pid=$1
    local rc=$2

    # Stop Huey gracefully
    kill -2 $pid
    wait $pid

    if [ $rc -eq 0 ]; then
        echo "test status: SUCCESS"
    else
        echo "test status: FAILURE $rc" >&2
    fi
    exit $rc
}

SCRIPT=task_queue.py
CONF_YAML=sample.yaml

rm -rf huey.log cluster_queue.db*
huey_consumer.py task_queue.huey -k process -l huey.log &
pidHuey=$!
sleep 10

declare -a tasks

for i in {0..9}; do
    tasks[$i]="$(python3 $SCRIPT -a < $CONF_YAML)"
done

while [[ -z $(python3 $SCRIPT -l | grep "RUNNING") ]]; do
    printf "Wait first to become RUNNING...\n"
    sleep 1
done

all_enqueued=$(python3 $SCRIPT -l | wc -l)
[[ all_enqueued -eq 10 ]] || stop_and_exit $pidHuey 1

# Cancel pending tasks
canceled1=$(python3 $SCRIPT -d ${tasks[1]%% *})
[[ -n "$(echo $canceled1 | grep 'REVOKED')" ]] || stop_and_exit $pidHuey 1

canceled2=$(python3 $SCRIPT -d ${tasks[8]%% *})
[[ -n "$(echo $canceled2 | grep 'REVOKED')" ]] || stop_and_exit $pidHuey 1

canceled3=$(python3 $SCRIPT -d ${tasks[3]%% *})
[[ -n "$(echo $canceled3 | grep 'REVOKED')" ]] || stop_and_exit $pidHuey 1

# Add one more
another_task="$(python3 $SCRIPT -a < $CONF_YAML)"

# Abort running task
running=$(python3 $SCRIPT -l | grep "RUNNING")
aborted=$(python3 $SCRIPT -d ${running%% *})
[[ -n "$(echo $aborted | grep 'ABORTED')" ]] || stop_and_exit $pidHuey 1

sec=0
while [[ $(python3 $SCRIPT -r | wc -l) -ne 11 ]]; do
    printf "($sec s) Wait all tasks are FINISHED...\r"
    sleep 1
    sec=$((sec+1))
    [[ $sec -le 50 ]] || stop_and_exit $pidHuey 1
done

# aborted + 3*canceled = 4
[[ $(python3 $SCRIPT -r | grep -E "^r:" | wc -l) -eq 4 ]] || stop_and_exit $pidHuey 1
stop_and_exit $pidHuey 0
