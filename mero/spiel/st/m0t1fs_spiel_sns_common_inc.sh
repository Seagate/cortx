#!/usr/bin/env bash
M0_SRC_DIR=`readlink -f $0`
M0_SRC_DIR=${M0_SRC_DIR%/*/*/*}

. $M0_SRC_DIR/utils/functions # die, sandbox_init, report_and_exit
. $M0_SRC_DIR/m0t1fs/linux_kernel/st/common.sh
. $M0_SRC_DIR/m0t1fs/linux_kernel/st/m0t1fs_common_inc.sh
. $M0_SRC_DIR/m0t1fs/linux_kernel/st/m0t1fs_client_inc.sh
. $M0_SRC_DIR/m0t1fs/linux_kernel/st/m0t1fs_server_inc.sh
. $M0_SRC_DIR/m0t1fs/linux_kernel/st/common_service_fids_inc.sh
. $M0_SRC_DIR/m0t1fs/linux_kernel/st/m0t1fs_sns_common_inc.sh

###################################################
# SNS repair is only supported in COPYTOOL mode,
# because ios need to hash gfid to mds. In COPYTOOL
# mode, filename is the string format of gfid.
###################################################
files=(
	0:10000
	0:10001
	0:10002
	0:10003
	0:10004
	0:10005
	0:10006
	0:10007
	0:10008
	0:10009
	0:10010
	0:10011
)

unit_size=(
	4
	8
	16
	32
	64
	128
	256
	512
	1024
	2048
	2048
	2048
)

file_size=(
	500
	700
	300
	0
	400
	0
	600
	200
	100
	60
	60
	60
)

N=3
K=3
P=15
stride=32
src_bs=10M
src_count=2

verify()
{
	echo "verifying ..."
	for ((i=0; i < ${#files[*]}; i++)) ; do
		local_read $((${unit_size[$i]} * 1024)) ${file_size[$i]} || return $?
		read_and_verify ${files[$i]} $((${unit_size[$i]} * 1024)) ${file_size[$i]} || return $?
	done

	echo "file verification sucess"
}

SPIEL_FIDS_LIST="
fids = {'profile'       : Fid(0x7000000000000001, 0),
        'pool'          : Fid(0x6f00000000000001, 9),
        'ios'           : Fid(0x7300000000000001, 3),
}"

SPIEL_RCONF_START="
print ('Hello, start to run Mero embedded python')

if spiel.cmd_profile_set(str(fids['profile'])):
    sys.exit('cannot set profile {0}'.format(fids['profile']))

if spiel.rconfc_start():
    sys.exit('cannot start rconfc')
"

SPIEL_RCONF_STOP="
spiel.rconfc_stop()
print ('----------Done------------')
"

PYTHON_STUFF=python_files.txt

spiel_prepare()
{
	local LNET_NID=`lctl list_nids | head -1`
	local SPIEL_CLIENT_ENDPOINT="$LNET_NID:12345:34:1001"
	local SPIEL_HA_ENDPOINT="$LNET_NID:12345:34:1"
	SPIEL_OPTS=" -l $M0_SRC_DIR/mero/.libs/libmero.so --client $SPIEL_CLIENT_ENDPOINT --ha $SPIEL_HA_ENDPOINT"

	export SPIEL_OPTS=$SPIEL_OPTS
	export SPIEL_FIDS_LIST=$SPIEL_FIDS_LIST

	echo SPIEL_OPTS=$SPIEL_OPTS
	echo SPIEL_FIDS_LIST=$SPIEL_FIDS_LIST

	# install "mero" Python module required by m0spiel tool
	cd $M0_SRC_DIR/utils/spiel
	python setup.py install --record $PYTHON_STUFF > /dev/null ||\
		die 'Cannot install Python "mero" module'
	cd -
}

spiel_cleanup()
{
	cd $M0_SRC_DIR/utils/spiel
	cat $PYTHON_STUFF | xargs rm -rf
	rm -rf build/ $PYTHON_STUFF
	cd -
}

spiel_sns_repair_start()
{
echo $M0_SRC_DIR/utils/spiel/m0spiel $SPIEL_OPTS
    $M0_SRC_DIR/utils/spiel/m0spiel $SPIEL_OPTS <<EOF
$SPIEL_FIDS_LIST

$SPIEL_RCONF_START

rc = spiel.sns_repair_start(fids['pool'])
print ("sns repair start rc: " + str(rc))

$SPIEL_RCONF_STOP
EOF
}

spiel_sns_repair_abort()
{
echo $M0_SRC_DIR/utils/spiel/m0spiel $SPIEL_OPTS
    $M0_SRC_DIR/utils/spiel/m0spiel $SPIEL_OPTS <<EOF
$SPIEL_FIDS_LIST

$SPIEL_RCONF_START

rc = spiel.sns_repair_abort(fids['pool'])
print ("sns repair abort rc: " + str(rc))

$SPIEL_RCONF_STOP
EOF
}

spiel_sns_repair_quiesce()
{
echo $M0_SRC_DIR/utils/spiel/m0spiel $SPIEL_OPTS
    $M0_SRC_DIR/utils/spiel/m0spiel $SPIEL_OPTS <<EOF
$SPIEL_FIDS_LIST

$SPIEL_RCONF_START

rc = spiel.sns_repair_quiesce(fids['pool'])
print ("sns repair quiesce rc: " + str(rc))

$SPIEL_RCONF_STOP
EOF
}

spiel_sns_repair_continue()
{
echo $M0_SRC_DIR/utils/spiel/m0spiel $SPIEL_OPTS
    $M0_SRC_DIR/utils/spiel/m0spiel $SPIEL_OPTS <<EOF
$SPIEL_FIDS_LIST

$SPIEL_RCONF_START

rc = spiel.sns_repair_continue(fids['pool'])
print ("sns repair continue rc: " + str(rc))

$SPIEL_RCONF_STOP
EOF
}

spiel_wait_for_sns_repair()
{
echo $M0_SRC_DIR/utils/spiel/m0spiel $SPIEL_OPTS
    $M0_SRC_DIR/utils/spiel/m0spiel $SPIEL_OPTS <<EOF
import time
$SPIEL_FIDS_LIST

$SPIEL_RCONF_START

one_status = SpielSnsStatus()
ppstatus = pointer(one_status)
active = 0
while (1):
    active = 0
    rc = spiel.sns_repair_status(fids['pool'], ppstatus)
    print ("sns repair status responded servers: " + str(rc))
    for i in range(0, rc):
        print "status of ", ppstatus[i].sss_fid, " is: ", ppstatus[i].sss_state
        if (ppstatus[i].sss_state == 2) :
            print "sns is still active on ", ppstatus[i].sss_fid
            active = 1
    if (active == 0):
        break;
    time.sleep(3)

$SPIEL_RCONF_STOP
EOF
}

spiel_sns_rebalance_start()
{
echo $M0_SRC_DIR/utils/spiel/m0spiel $SPIEL_OPTS
    $M0_SRC_DIR/utils/spiel/m0spiel $SPIEL_OPTS <<EOF
$SPIEL_FIDS_LIST

$SPIEL_RCONF_START

rc = spiel.sns_rebalance_start(fids['pool'])
print ("sns rebalance start rc: " + str(rc))

$SPIEL_RCONF_STOP
EOF
}

spiel_sns_rebalance_quiesce()
{
echo $M0_SRC_DIR/utils/spiel/m0spiel $SPIEL_OPTS
    $M0_SRC_DIR/utils/spiel/m0spiel $SPIEL_OPTS <<EOF
$SPIEL_FIDS_LIST

$SPIEL_RCONF_START

rc = spiel.sns_rebalance_quiesce(fids['pool'])
print ("sns rebalance quiesce rc: " + str(rc))

$SPIEL_RCONF_STOP
EOF
}

spiel_sns_rebalance_continue()
{
echo $M0_SRC_DIR/utils/spiel/m0spiel $SPIEL_OPTS
    $M0_SRC_DIR/utils/spiel/m0spiel $SPIEL_OPTS <<EOF
$SPIEL_FIDS_LIST

$SPIEL_RCONF_START

rc = spiel.sns_rebalance_continue(fids['pool'])
print ("sns rebalance continue rc: " + str(rc))

$SPIEL_RCONF_STOP
EOF
}

spiel_wait_for_sns_rebalance()
{
echo $M0_SRC_DIR/utils/spiel/m0spiel $SPIEL_OPTS
    $M0_SRC_DIR/utils/spiel/m0spiel $SPIEL_OPTS <<EOF
import time
$SPIEL_FIDS_LIST

$SPIEL_RCONF_START

one_status = SpielSnsStatus()
ppstatus = pointer(one_status)
active = 0
while (1):
    active = 0
    rc = spiel.sns_rebalance_status(fids['pool'], ppstatus)
    print ("sns rebalance status responded servers: " + str(rc))
    for i in range(0, rc):
        print "status of ", ppstatus[i].sss_fid, " is: ", ppstatus[i].sss_state
        if (ppstatus[i].sss_state == 2) :
            print "sns is still active on ", ppstatus[i].sss_fid
            active = 1
    if (active == 0):
        break;
    time.sleep(3)

$SPIEL_RCONF_STOP
EOF
}

spiel_sns_rebalance_abort()
{
echo $M0_SRC_DIR/utils/spiel/m0spiel $SPIEL_OPTS
    $M0_SRC_DIR/utils/spiel/m0spiel $SPIEL_OPTS <<EOF
$SPIEL_FIDS_LIST

$SPIEL_RCONF_START

rc = spiel.sns_rebalance_abort(fids['pool'])
print ("rebalance abort rc: " + str(rc))

$SPIEL_RCONF_STOP
EOF
}

