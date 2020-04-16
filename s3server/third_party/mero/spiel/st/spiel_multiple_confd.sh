#!/usr/bin/env bash
set -eu

#set -x
#export PS4='+ ${FUNCNAME[0]:+${FUNCNAME[0]}():}line ${LINENO}: '

## CAUTION: This path will be removed by superuser.
SANDBOX_DIR=${SANDBOX_DIR:-/var/mero/sandbox.spiel-st}

M0_TRACE_IMMEDIATE_MASK=${M0_TRACE_IMMEDIATE_MASK:-all}
M0_TRACE_LEVEL=${M0_TRACE_LEVEL:-warn}
M0_TRACE_PRINT_CONTEXT=${M0_TRACE_PRINT_CONTEXT:-}

MAX_RPC_MSG_SIZE=163840
TM_MIN_RECV_QUEUE_LEN=2
INSTALLED_FILES=cleanup-on-quit.txt

error() { echo "$@" >&2; stop 1; }
say() { echo "$@" | tee -a $SANDBOX_DIR/confd1/m0d.log; }

M0_SRC_DIR=`readlink -f $0`
M0_SRC_DIR=${M0_SRC_DIR%/*/*/*}

. $M0_SRC_DIR/utils/functions # die, sandbox_init, report_and_exit

## Path to the file with configuration string for confd.
CONF_FILE=$SANDBOX_DIR/confd1/conf.txt

PROC_FID1="<0x7200000000000001:0>"
PROC_FID2="<0x7200000000000001:1>"
PROC_FID3="<0x7200000000000001:2>"
PROF_OPT="<0x7000000000000001:0>"

PYTHON_BOILERPLATE="
if spiel.cmd_profile_set('$PROF_OPT'):
    sys.exit('cannot set profile $PROF_OPT')

if spiel.rconfc_start():
    sys.exit('cannot start rconfc')"

start() {
    # install "mero" Python module required by m0spiel tool
    cd $M0_SRC_DIR/utils/spiel
    python setup.py install --record $INSTALLED_FILES > /dev/null ||
        die 'Cannot install Python "mero" module'
    sandbox_init
    _init
    stub_confdb | $M0_SRC_DIR/utils/m0confgen >$CONF_FILE
}

stop() {
    local rc=${1:-$?}

    trap - EXIT
    killall -q lt-m0d && wait || rc=$?
    _fini
    if [ $rc -eq 0 ]; then
        sandbox_fini
    else
        say "Spiel test FAILED: $rc"
        exit $rc
    fi
}

_init() {
    lnet_up
    m0_modules_insert
    mkdir -p $SANDBOX_DIR/confd{1..3}
}

_fini() {
    m0_modules_remove
    cd $M0_SRC_DIR/utils/spiel
    cat $INSTALLED_FILES | xargs rm -rf
    rm -rf build/ $INSTALLED_FILES
}

stub_confdb() {
    cat <<EOF
(root-0 verno=1 rootfid=(11, 22) mdpool=pool-0 imeta_pver=(0, 0)
    mdredundancy=2 params=["pool_width=3", "nr_data_units=1",
                           "nr_parity_units=1", "unit_size=4096"]
    nodes=[node-0] sites=[site-2] pools=[pool-0]
    profiles=[profile-0] fdmi_flt_grps=[])
(profile-0 pools=[pool-0])
(node-0 memsize=16000 nr_cpu=2 last_state=3 flags=2
    processes=[process-0, process-1, process-2])
(process-0 cores=[3] mem_limit_as=0 mem_limit_rss=0 mem_limit_stack=0
    mem_limit_memlock=0 endpoint="$M0D1_ENDPOINT"
    services=[service-0, service-3, service-4])
(process-1 cores=[3] mem_limit_as=0 mem_limit_rss=0 mem_limit_stack=0
    mem_limit_memlock=0 endpoint="$M0D2_ENDPOINT"
    services=[service-6, service-7])
(process-2 cores=[3] mem_limit_as=0 mem_limit_rss=0 mem_limit_stack=0
    mem_limit_memlock=0 endpoint="$M0D3_ENDPOINT"
    services=[service-8, service-9])
(service-0 type=@M0_CST_RMS endpoints=["$M0D1_ENDPOINT"] params=[] sdevs=[])
(service-3 type=@M0_CST_MDS endpoints=["$M0D1_ENDPOINT"] params=[]
    sdevs=[sdev-0])
(service-4 type=@M0_CST_CONFD endpoints=["$M0D1_ENDPOINT"] params=[] sdevs=[])
(service-6 type=@M0_CST_RMS endpoints=["$M0D2_ENDPOINT"] params=[] sdevs=[])
(service-7 type=@M0_CST_CONFD endpoints=["$M0D2_ENDPOINT"] params=[] sdevs=[])
(service-8 type=@M0_CST_RMS endpoints=["$M0D3_ENDPOINT"] params=[] sdevs=[])
(service-9 type=@M0_CST_CONFD endpoints=["$M0D3_ENDPOINT"] params=[] sdevs=[])
(pool-0 pver_policy=0 pvers=[pver-0, pver_f-11])
(pver-0 N=2 K=1 P=4 tolerance=[0, 0, 0, 0, 1] sitevs=[objv-2:0])
(pver_f-11 id=0 base=pver-0 allowance=[0, 0, 0, 0, 1])
(objv-2:0 real=site-2 children=[objv-0])
(objv-0 real=rack-0 children=[objv-1])
(objv-1 real=enclosure-0 children=[objv-2])
(objv-2 real=controller-0 children=[])
(site-2 racks=[rack-0] pvers=[pver-0])
(rack-0 encls=[enclosure-0] pvers=[pver-0])
(enclosure-0 ctrls=[controller-0] pvers=[pver-0])
(controller-0 node=node-0 drives=[] pvers=[pver-0])
(sdev-0 dev_idx=0 iface=4 media=1 bsize=4096 size=596000000000 last_state=3
    flags=4 filename="/dev/sdev0")
EOF
}

confd_start() {
    local idx=$1
    local ep=M0D${idx}_ENDPOINT
    local fid=PROC_FID$idx
    local path=$SANDBOX_DIR/confd$idx
    local OPTS="-F -D $path/db -T AD -S $path/stobs\
    -A linuxstob:$path/addb-stobs -e lnet:${!ep}\
    -m $MAX_RPC_MSG_SIZE -q $TM_MIN_RECV_QUEUE_LEN -c $CONF_FILE\
    -w 3 -f ${!fid}"

    echo "--- `date` ---" >>$path/m0d.log
    cd $path

    echo $M0_SRC_DIR/utils/mkfs/m0mkfs $OPTS
    $M0_SRC_DIR/utils/mkfs/m0mkfs $OPTS >>$path/mkfs.log ||
    error 'm0mkfs failed'

    echo $M0_SRC_DIR/mero/m0d $OPTS
    $M0_SRC_DIR/mero/m0d $OPTS >>$path/m0d.log 2>&1 &
    local PID=$!
    sleep 10
    kill -0 $PID 2>/dev/null ||
    error "Failed to start m0d. See $path/m0d.log for details."
}

connect_to_confds() {
    ### Successful m0spiel start means that its internal rconfc instance is
    ### started, i. e. quorum is reached. Successful stopping of the tool means
    ### that rconfc's herd list was finalised without errors.
    $M0_SRC_DIR/utils/spiel/m0spiel -l $M0_SRC_DIR/mero/.libs/libmero.so\
	    --client $SPIEL_ENDPOINT <<EOF
$PYTHON_BOILERPLATE

spiel.rconfc_stop()
EOF
}

[ `id -u` -eq 0 ] || die 'Must be run by superuser'

trap stop EXIT

echo "Prepare"
start

say "First confd start"
confd_start 1 || stop

say "Second confd start"
confd_start 2 || stop

say "Third confd start"
confd_start 3 || stop

say "Test"
connect_to_confds || stop

say "Stop"
stop
report_and_exit multiple_confd $?
