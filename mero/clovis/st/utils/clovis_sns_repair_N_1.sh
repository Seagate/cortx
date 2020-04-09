#!/bin/bash

#set -x

clovis_st_util_dir=$( cd "$(dirname "$0")" ; pwd -P )

. $clovis_st_util_dir/clovis_sns_common.sh

N=1
K=2
P=14
stride=4

main()
{
	clovis_sns_repreb $N $K $P $stride

	return $rc
}

echo "SNS Repair/Rebalance N=1 Test ... "
trap unprepare EXIT
main
report_and_exit sns-repair-rebalance-N-1 $?
