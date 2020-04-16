#!/usr/bin/env bash

BETOOL="sudo utils/m0run m0betool"
LT_BETOOL="lt-m0betool"
KILL_INTERVAL=60

function kill_betool()
{
	kill -SIGKILL `pidof $LT_BETOOL`
}

trap kill_betool EXIT

rm -rf /var/mero/m0betool
$BETOOL st mkfs
i=0
while true; do
	$BETOOL st run &
	PID="$!"
	sleep $KILL_INTERVAL
	kill -SIGKILL `pidof $LT_BETOOL`
	KILL_STATUS=$?
	wait
	if [ $KILL_STATUS -ne 0 ]; then
		break
	fi
	((++i))
	echo "iteration #$i"
done
