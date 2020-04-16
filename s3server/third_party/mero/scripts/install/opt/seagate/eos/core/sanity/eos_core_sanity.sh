#/*
# * COPYRIGHT 2019 XYRATEX TECHNOLOGY LIMITED
# *
# * THIS DRAWING/DOCUMENT, ITS SPECIFICATIONS, AND THE DATA CONTAINED
# * HEREIN, ARE THE EXCLUSIVE PROPERTY OF XYRATEX TECHNOLOGY
# * LIMITED, ISSUED IN STRICT CONFIDENCE AND SHALL NOT, WITHOUT
# * THE PRIOR WRITTEN PERMISSION OF XYRATEX TECHNOLOGY LIMITED,
# * BE REPRODUCED, COPIED, OR DISCLOSED TO A THIRD PARTY, OR
# * USED FOR ANY PURPOSE WHATSOEVER, OR STORED IN A RETRIEVAL SYSTEM
# * EXCEPT AS ALLOWED BY THE TERMS OF XYRATEX LICENSES AND AGREEMENTS.
# *
# * YOU SHOULD HAVE RECEIVED A COPY OF XYRATEX'S LICENSE ALONG WITH
# * THIS RELEASE. IF NOT PLEASE CONTACT A XYRATEX REPRESENTATIVE
# * http://www.xyratex.com/contact
# *
# * Original author: Abhishek Saha <abhishek.saha@seagate.com>
# * Original creation date: 4-Feb-2020
# */

#!/bin/bash
#set -x
set -e
# This script starts and stops eos core singlenode and performs some Object and
# KV Litmus tests.
# This script is heavily influenced by "clovis-sample-apps/scripts/c0appzrcgen"
# This script should only be run before bootstrap (that does mkfs),
# and it will never be started after Mero mkfs is complete

conf="/etc/mero/conf.xc"
currdir=$(pwd)
timestamp=$(date +%d_%b_%Y_%H_%M)
SANITY_SANDBOX_DIR="/var/mero/sanity_$timestamp"
base_port=301
IP=""
port=""
local_endpoint=""
ha_endpoint=""
profile_fid=""
process_fid=""

start_singlenode()
{
	mkdir -p $SANITY_SANDBOX_DIR
	cd $SANITY_SANDBOX_DIR

	# setup eos core singlenode
	m0singlenode activate
	m0setup -cv -d $SANITY_SANDBOX_DIR -m $SANITY_SANDBOX_DIR
	m0setup -N 1 -K 0 -P 8 -s 8 -Mv -d $SANITY_SANDBOX_DIR \
		-m $SANITY_SANDBOX_DIR --no-m0t1fs

	# start eos core
	m0singlenode start
}

stop_singlenode()
{
	# stop eos core
	m0singlenode stop
	m0setup -cv -d $SANITY_SANDBOX_DIR -m $SANITY_SANDBOX_DIR
	cd $currdir

	# cleanup remaining mero-services
	systemctl start mero-cleanup

	# remove sanity test sandbox directory
	if [[ $1 == "cleanup" ]]
	then
		rm -rf $SANITY_SANDBOX_DIR
	fi
}

ip_generate()
{
	IP=$(lctl list_nids | cut  -f 1 -d' ' | head -n 1)
	if [[ ! ${IP} ]]; then
		(>&2 echo 'error! m0singlenode not running.')
		(>&2 echo 'start m0singlenode')
		exit
	fi
}

node_sanity_check()
{
	if [ ! -f $conf ]
	then
		echo "Error: $conf is missing, it should already be created by m0setup"
		return 1
	fi
	string=`grep $IP $conf | cut -d'"' -f 2 | cut -d ':' -f 1`
	set -- $string
	ip=`echo $1`
	if [ "$ip" != "$IP" ]
	then
		echo $ip
		echo $IP
		echo "Change in configuration format"
		return 1
	fi
	return 0
}

unused_port_get()
{
	hint=$1
	port_list=`grep $IP $conf | cut -d '"' -f 2 | cut -d ':' -f 4`
	while [[ $port_list = *"$hint"* ]]
	do
		hint=$(($hint+1))
	done
	port=$hint
}

generate_endpoints()
{
	ip_generate
	node_sanity_check

	if [ $? -ne 0 ]
	then
		return 1
	fi

	unused_port_get "$base_port"
	local_endpoint="${IP}:12345:44:$port"
	echo "Local endpoint: $local_endpoint"

	ha_endpoint="${IP}:12345:45:1"
	echo "HA endpoint: $ha_endpoint"

	profile_fid='<0x7000000000000001:0>'
	echo "Profile FID: $profile_fid"

	process_fid='<0x7200000000000000:0>'
	echo "Process FID: $process_fid"
}

error_handling()
{
	echo $1
	stop_singlenode
	exit $2
}

object_io_test()
{
	echo "Running Object IO tests"
	obj_id1="20:20"
	obj_id2="20:22"
	blk_size="4k"
	blk_count="200"
	src_file="/tmp/src"
	dest_file="/tmp/dest"
        echo $blk_size $blk_count
	dd if="/dev/urandom" of=$src_file bs=$blk_size count=$blk_count || {
		error_handling "dd command failed" $?
	}
	endpoint_opts="-l $local_endpoint -H $ha_endpoint -p $profile_fid \
		       -P $process_fid"
	rm -f $dest_file
	c0cp $endpoint_opts -o $obj_id1 -s $blk_size -c $blk_count $src_file || {
		error_handling "Failed to write object" $?
	}
	c0cat $endpoint_opts -o $obj_id1 -s $blk_size -c $blk_count > $dest_file || {
		error_handling "Failed to read object" $?
	}
	diff $src_file $dest_file || {
		error_handling "Files differ" $?
	}
	c0unlink $endpoint_opts -o $obj_id1
	rm -f $dest_file

	c0touch $endpoint_opts -o $obj_id2
	c0cp $endpoint_opts -o $obj_id2 -s $blk_size -c $blk_count $src_file -u || {
		error_handling "Failed to write object" $?
	}
	c0cat $endpoint_opts -o $obj_id2 -s $blk_size -c $blk_count > $dest_file || {
		error_handling "Failed to read object" $?
	}
	diff $src_file $dest_file || {
		error_handling "Files differ" $?
	}
	c0unlink $endpoint_opts -o $obj_id2
	rm -f $dest_file $src_file
	blk_size_dd="1M"
	blk_size="1m"
	blk_count="16"
	src_file="/tmp/src_1M"
	dest_file="/tmp/dest"
        echo $blk_size $blk_count
	dd if="/dev/urandom" of=$src_file bs=$blk_size_dd count=$blk_count || {
		error_handling "dd command failed" $?
	}
	endpoint_opts="-l $local_endpoint -H $ha_endpoint -p $profile_fid \
		       -P $process_fid"
	c0cp $endpoint_opts -o $obj_id1 -s $blk_size -c $blk_count $src_file -L 9 || {
		error_handling "Failed to write object" $?
	}
	c0cat $endpoint_opts -o $obj_id1 -s $blk_size -c $blk_count -L 9 > $dest_file || {
		error_handling "Failed to read object" $?
	}
	diff $src_file $dest_file || {
		error_handling "Files differ" $?
	}
	c0unlink $endpoint_opts -o $obj_id1
	rm -f $dest_file $src_file
}

kv_test()
{
	echo "Running KV tests"
	index_id1="<0x780000000000000b:1>"
	index_id2="<0x780000000000000b:2>"
	index_id3="<0x780000000000000b:3>"
	endpoint_opts="-l $local_endpoint -h $ha_endpoint -p $profile_fid -f $process_fid"
	rm -f keys.txt vals.txt
	echo "m0clovis"
	m0clovis $endpoint_opts index genv 10 20 "keys.txt" &> "/dev/null"
	m0clovis $endpoint_opts index genv 10 20 "vals.txt" &> "/dev/null"
	m0clovis $endpoint_opts index create $index_id1 || {
		error_handling "Failed to create index $index_id1" $?
	}
	m0clovis $endpoint_opts index create $index_id2 || {
		error_handling "Failed to create index $index_id2" $?
	}
	m0clovis $endpoint_opts index create $index_id3 || {
		error_handling "Failed to create index $index_id3" $?
	}
	m0clovis $endpoint_opts index list $index_id1 3 || {
		error_handling "Failed to list indexes" $?
	}
	m0clovis $endpoint_opts index lookup $index_id2 || {
		error_handling "Failed to lookup index $index_id2" $?
	}
	m0clovis $endpoint_opts index drop $index_id1 || {
		error_handling "Failed to drop index $index_id1" $?
	}
	m0clovis $endpoint_opts index drop $index_id2 || {
		error_handling "Failed to drop index $index_id2" $?
	}
	m0clovis $endpoint_opts index put $index_id3 @keys.txt @vals.txt || {
		error_handling "Failed to put KV on index $index_id3" $?
	}
	m0clovis $endpoint_opts index get $index_id3 @keys.txt || {
		error_handling "Failed to get KV on index $index_id3" $?
	}
	m0clovis $endpoint_opts index next $index_id3 "$(head -n 1 keys.txt | cut -f 2- -d ' ')" 1 || {
		error_handling "Failed to get next KV on index $index_id3" $?
	}
	m0clovis $endpoint_opts index del $index_id3 @keys.txt || {
		error_handling "Failed to delete KV on $index_id3" $?
	}
	m0clovis $endpoint_opts index drop $index_id3 || {
		error_handling "Failed to drop index $index_id3" $?
	}
	rm -f keys.txt vals.txt
	echo "c0mt test"
        c0mt $endpoint_opts
}

m0spiel_test()
{
	echo "m0_filesystem_stats"
	libmero_path="/usr/lib64/libmero.so"
    	[[ ! -s $libmero_path ]] && libmero_path=$currdir/mero/.libs/libmero.so
	format_profile_fid=$(echo $profile_fid | sed 's/.*<\(.*\)>/\1/' | sed 's/:/,/')
	/usr/bin/m0_filesystem_stats -s $ha_endpoint -p $format_profile_fid -c ${ha_endpoint}000 -l $libmero_path
	if [ $? -ne 0 ] ; then
		error_handling "Failed to run m0_filesystem_stats " $?
	fi
}

run_tests()
{
	# Run litmus test
	object_io_test
	kv_test
	m0spiel_test
}

main()
{
	start_singlenode
	generate_endpoints
	if [ $? -ne 0 ]
	then
		return 1
	fi
	run_tests
	stop_singlenode "cleanup"
}

main
