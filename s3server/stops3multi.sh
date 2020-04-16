pids="$(ps -A | grep s3server | awk '{print $1}' | paste -s)"
echo $pids

kill $pids
