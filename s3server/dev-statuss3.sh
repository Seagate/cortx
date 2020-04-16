#!/bin/sh

MAX_S3_INSTANCES_NUM=20
instance=1
while [[ $instance -le $MAX_S3_INSTANCES_NUM ]]
do
  is_up=$(./iss3up.sh $instance)
  rc=$?
  if [[ "$rc" -eq 0 ]]; then
    echo $is_up
  else
    exit 1
  fi
  ((instance++))
done
