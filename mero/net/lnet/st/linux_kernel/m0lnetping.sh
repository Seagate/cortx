#!/usr/bin/env bash

# Small wrapper to run kernel lnetping ST

if [ "$(id -u)" -ne 0 ]; then
    echo "Must be run as root"
    exit 1
fi

usage() {
    echo "Usage: $0 {-s | -c | -s -c}"
    echo "   [-b #Bufs] [-l #Loops] [-n #Threads] [-o MessageTimeout]"
    echo "   [-O BulkTimeout] [-m MsgSize] [-d BulkSize] [-D ActiveDelay]"
    echo "   [-B #ReceiveBufs] [-M MaxReceiveMsgs] [-R ReceiveMsgSize]"
    echo "   [-i ClientNetwork] [-p ClientPortal] [-t ClientTMID]"
    echo "   [-I ServerNetwork] [-P ServerPortal] [-T ServerTMID] [-A]"
    echo "   [-x ClientDebug] [-X ServerDebug] [-q] [-v VerbosityLevel]"
    echo "Flags:"
    echo "-A  Async event processing (old style)"
    echo "-B  Number of receive buffers (server only)"
    echo "-D  Server active bulk delay"
    echo "-I  Server network interface (ip@intf)"
    echo "-M  Max receive messages in a single buffer (server only)"
    echo "-O  Bulk timeout in seconds"
    echo "-P  Server portal"
    echo "-R  Receive message max size (server only)"
    echo "-T  Server TMID"
    echo "-X  Server debug"
    echo "-b  Number of buffers"
    echo "-c  Run client only"
    echo "-d  Bulk data size"
    echo "-i  Client network interface (ip@intf)"
    echo "-l  Loops to run"
    echo "-m  Message size (client only)"
    echo "-n  Number of client threads"
    echo "-o  Message send timeout in seconds"
    echo "-p  Client portal"
    echo "-q  Not verbose"
    echo "-s  Run server only"
    echo "-t  Client base TMID - default is dynamic"
    echo "-v  Verbosity level"
    echo "-x  Client debug"
    echo "By default the client and server are configued to use the first LNet"
    echo "network interface returned by lctl."
}

d="`git rev-parse --show-cdup`"
if [ -n "$d" ]; then
    cd "$d"
fi

modprobe lnet
if [ $? -ne 0 ]; then
    echo "The lnet module is not loaded"
    exit 1
fi
lctl network up
if [ $? -ne 0 ] ; then
    echo "LNet network not enabled"
    exit 1
fi

# use the first NID configured
NID=`lctl list_nids | head -n 1`
if [ -z "$NID" ] ; then
    echo "No networks available"
    exit 1
fi

server_nid=$NID
client_nid=$NID

Pquiet=
Pverbose=
Pserver_only=
Pclient_only=
Pasync_events=
Pnr_bufs=
Pnr_recv_bufs=
Ploops=
Pbulk_size="bulk_size=30K"
Pbulk_timeout="bulk_timeout=20"
Pmsg_timeout="msg_timeout=5"
Pactive_bulk_delay=
Pnr_clients=
Pclient_network="client_network=$client_nid"
Pclient_portal=
Pclient_tmid=
Pclient_debug=
Psend_msg_size=
Pserver_min_recv_size=
Pserver_max_recv_msgs=
Pserver_network="server_network=$server_nid"
Pserver_portal=
Pserver_tmid=
Pserver_debug=

while [ $# -gt 0 ]; do
    FLAG=$1; shift
    has_sarg=0
    has_narg=0
    case $FLAG in
	(-c) Pclient_only="client_only";;
	(-s) Pserver_only="server_only";;
	(-A) Pasync_events="async_events";;
	(-q) Pquiet="quiet" ;;
	(-B|-D|-M|-O|-P|-R|-T|-X|-b|-d|-l|-m|-n|-o|-p|-t|-v|-x) has_narg=1;;
	(-I|-i) has_sarg=1;;
	(-h|--help) usage; exit 0;;
	(*) echo "Error: Unknown argument $FLAG"; echo "Use -h for help"; exit 1;;
    esac
    if [ $has_sarg -eq 0 -a $has_narg -eq 0 ]; then
	continue;
    fi
    if [ $# -eq 0 ] ; then
	echo "Error: $FLAG needs an argument"
	exit 1;
    fi
    if [ $has_narg -eq 1 ] ; then
	case $1 in
	    ([0-9]*) ;;
	    (*) echo "Error: $FLAG needs a numeric argument"
	        exit 1
		;;
	esac
    fi
    case $FLAG in
	(-B) Pnr_recv_bufs="nr_recv_bufs=$1";;
	(-D) Pactive_bulk_delay="active_bulk_delay $1";;
	(-I) Pserver_network="server_network=$1"; server_nid=$1;;
	(-M) Pserver_max_recv_msgs="server_max_recv_msgs=$1";;
	(-O) Pbulk_timeout="bulk_timeout=$1";;
	(-P) Pserver_portal="server_portal=$1";;
	(-R) Pserver_min_recv_size="server_min_recv_size=$1";;
	(-T) Pserver_tmid="server_tmid=$1";;
	(-X) Pserver_debug="server_debug=$1";;
	(-b) Pnr_bufs="nr_bufs=$1";;
	(-d) Pbulk_size="bulk_size=$1";;
	(-i) Pclient_network="client_network=$1"; client_nid=$1;;
	(-l) Ploops="loops=$1";;
	(-m) Psend_msg_size="send_msg_size=$1";;
	(-n) Pnr_clients="nr_clients=$1";;
	(-o) Pmsg_timeout="msg_timeout=$1";;
	(-p) Pclient_portal="client_portal=$1";;
	(-t) Pclient_tmid="client_tmid=$1";;
	(-v) Pverbose="verbose=$1";;
	(-x) Pclient_debug="client_debug=$1";;
    esac
    shift
done

if [ -z "$Pserver_only" -a -z "$Pclient_only" ] ; then
    echo "Error: Specify if server, client or both roles to be run locally"
    exit 1
fi

# Server parameters
SPARM="$Pserver_only $Pserver_network $Pserver_portal $Pserver_tmid \
$Pnr_recv_bufs $Pserver_max_recv_msgs $Pserver_min_recv_size \
$Pasync_events $Pactive_bulk_delay $Pserver_debug"

# Client parameters
CPARM="$Pclient_only $Pclient_network $Pclient_portal $Pclient_tmid \
$Pnr_clients $Ploops $Psend_msg_size $Pclient_debug"

# Other parameters
OPARM="$Pquiet $Pverbose $Pnr_bufs $Pbulk_size $Pmsg_timeout $Pbulk_timeout"

echo $OPARM
echo $SPARM
if [ -n "$Pclient_only" ]; then
    echo $CPARM
    if [ $server_nid = $client_nid -a $client_nid != $NID ]; then
	echo "Error: Standalone client requires a remote server network"
	exit 1
    fi
fi

. m0t1fs/linux_kernel/st/common.sh

MODLIST="m0mero.ko"

log='/var/log/kern'
if [ ! -e "$log" ]; then
    log='/var/log/messages'
fi
tailseek=$(( $(stat -c %s "$log") + 1 ))

# insert ST module separately to pass parameters
STMOD=m0lnetping
unload_all() {
    echo "Aborted! Unloading kernel modules..."
    rmmod $STMOD
    modunload
    modunload_m0gf
}
trap unload_all EXIT

modload_m0gf || exit $?
modload || exit $?

insmod $STMOD.ko $OPARM $SPARM $CPARM
if [ $? -eq 0 ] ; then
    if [ -z "$Pclient_only" ] ; then
	msg="Enter EOF to stop the server"
	echo $msg
	while read LINE ; do
	    echo $msg
	done
    fi
    rmmod $STMOD
fi

modunload
modunload_m0gf

trap "" EXIT

sleep 1
tail -c+$tailseek "$log" | grep ' kernel: '

exit 0
