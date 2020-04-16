#!/bin/bash

# Compare results with last known good test results.

baseline_compare() {
  fname="$1"
  expected_bw="$2"
  if test -f "$fname"; then
    summary=`tail -n1 "$fname"`
    regex="^ *(WRITE|READ): bw=([0-9]+)MiB.*"
    if [[ $summary =~ $regex ]]; then
      bw="${BASH_REMATCH[2]}"
      diff=$((bw - expected_bw))
      diff_pct=$(( (diff * 100) / expected_bw ))
      printf "%-50s  expected %4d  actual %4d  diff %5d (%3d%%)\n" \
        $fname $expected_bw $bw $diff $diff_pct
    else
      echo "Cannot parse $fname:"
      echo "$summary"
    fi
  else
    echo "$fname missing"
  fi
}

echo 'Bandwidth baseline (in MiB/s)'
echo ' (as compared to measurements taken on i1u06-m06 at 10/21/2019, 2x4=7+1 configuration)'
echo ' (https://docs.google.com/spreadsheets/d/16NESrPSqnQ_u7ZbEP9pG5oy7615PPJUIa2GYzhrDWFs/edit#gid=249969707)'
baseline_compare 7volumes.rw-write.numjobs-1.bs-2m.fio-output     3295
baseline_compare 7volumes.rw-write.numjobs-16.bs-2m.fio-output    3192
baseline_compare 7volumes.rw-write.numjobs-1.bs-8m.fio-output     3289
baseline_compare 7volumes.rw-randwrite.numjobs-1.bs-2m.fio-output  610
baseline_compare 7volumes.rw-read.numjobs-1.bs-2m.fio-output      3500
baseline_compare 7volumes.rw-randread.numjobs-1.bs-2m.fio-output  3287
echo
baseline_compare 7volumes.rw-randwrite.numjobs-1.bs-2m.fio-output    610
baseline_compare 7volumes.rw-randwrite.numjobs-1.bs-1m.fio-output    617
baseline_compare 7volumes.rw-randwrite.numjobs-1.bs-512k.fio-output  615
baseline_compare 7volumes.rw-randwrite.numjobs-1.bs-4M.fio-output   3293
baseline_compare 7volumes.rw-randwrite.numjobs-1.bs-8m.fio-output   3284
