#!/usr/bin/env bash
set -eu

die() { echo "$@" >&2; exit 1; }

# rpc-ub() { sudo utils/ub.sh -t rpc-ub "$@"; }

### XXX Mock benchmark: generates benchmark output without running it.
rpc-ub() {
    [ $# -eq 2 -a "$1" = '-o' ] || die 'Wrong usage of rpc-ub'
    local OPTS="$2"

    {
	cat <<EOF
>>>>> 'msg_len=1024,nr_msgs=100' nr_conns '8 16 32 64 96'
----------[ nr_conns=8,msg_len=1024,nr_msgs=100 ]----------
-- round  1 rpc-ub[.]
	       bench: [   iter]    min    max    avg   std   sec/op   op/sec
set: rpc-ub
	         run: [      1]   1.74   1.74   1.74  0.00% 1.743e+00/5.738e-01
----------[ nr_conns=16,msg_len=1024,nr_msgs=100 ]----------
-- round  1 rpc-ub[.]
	       bench: [   iter]    min    max    avg   std   sec/op   op/sec
set: rpc-ub
	         run: [      1]   1.75   1.75   1.75  0.00% 1.749e+00/5.718e-01
----------[ nr_conns=32,msg_len=1024,nr_msgs=100 ]----------
-- round  1 rpc-ub[.]
	       bench: [   iter]    min    max    avg   std   sec/op   op/sec
set: rpc-ub
	         run: [      1]   2.48   2.48   2.48  0.00% 2.482e+00/4.029e-01
----------[ nr_conns=64,msg_len=1024,nr_msgs=100 ]----------
-- round  1 rpc-ub[.]
	       bench: [   iter]    min    max    avg   std   sec/op   op/sec
set: rpc-ub
	         run: [      1]  13.81  13.81  13.81  0.00% 1.381e+01/7.242e-02
----------[ nr_conns=96,msg_len=1024,nr_msgs=100 ]----------
-- round  1 rpc-ub[.]
	       bench: [   iter]    min    max    avg   std   sec/op   op/sec
set: rpc-ub
	         run: [      1] 260.08 260.08 260.08  0.00% 2.601e+02/3.845e-03

>>>>> 'nr_conns=96,nr_msgs=100' msg_len '64 128 256 512'
----------[ msg_len=64,nr_conns=96,nr_msgs=100 ]----------
-- round  1 rpc-ub[.]
	       bench: [   iter]    min    max    avg   std   sec/op   op/sec
set: rpc-ub
	         run: [      1]  67.36  67.36  67.36  0.00% 6.736e+01/1.484e-02
----------[ msg_len=128,nr_conns=96,nr_msgs=100 ]----------
-- round  1 rpc-ub[.]
	       bench: [   iter]    min    max    avg   std   sec/op   op/sec
set: rpc-ub
	         run: [      1]  47.41  47.41  47.41  0.00% 4.741e+01/2.109e-02
----------[ msg_len=256,nr_conns=96,nr_msgs=100 ]----------
-- round  1 rpc-ub[.]
	       bench: [   iter]    min    max    avg   std   sec/op   op/sec
set: rpc-ub
	         run: [      1]  44.83  44.83  44.83  0.00% 4.483e+01/2.230e-02
----------[ msg_len=512,nr_conns=96,nr_msgs=100 ]----------
-- round  1 rpc-ub[.]
	       bench: [   iter]    min    max    avg   std   sec/op   op/sec
set: rpc-ub
	         run: [      1]  69.11  69.11  69.11  0.00% 6.911e+01/1.447e-02

>>>>> 'nr_conns=32,nr_msgs=400' msg_len '256 512 1024 2048'
----------[ msg_len=256,nr_conns=32,nr_msgs=400 ]----------
-- round  1 rpc-ub[.]
	       bench: [   iter]    min    max    avg   std   sec/op   op/sec
set: rpc-ub
	         run: [      1]   4.77   4.77   4.77  0.00% 4.769e+00/2.097e-01
----------[ msg_len=512,nr_conns=32,nr_msgs=400 ]----------
-- round  1 rpc-ub[.]
	       bench: [   iter]    min    max    avg   std   sec/op   op/sec
set: rpc-ub
	         run: [      1]   6.03   6.03   6.03  0.00% 6.031e+00/1.658e-01
----------[ msg_len=1024,nr_conns=32,nr_msgs=400 ]----------
-- round  1 rpc-ub[.]
	       bench: [   iter]    min    max    avg   std   sec/op   op/sec
set: rpc-ub
	         run: [      1]   7.97   7.97   7.97  0.00% 7.970e+00/1.255e-01
----------[ msg_len=2048,nr_conns=32,nr_msgs=400 ]----------
-- round  1 rpc-ub[.]
	       bench: [   iter]    min    max    avg   std   sec/op   op/sec
set: rpc-ub
	         run: [      1]  14.69  14.69  14.69  0.00% 1.469e+01/6.806e-02

EOF
    } | awk -v OPTS=$OPTS '
$2 == OPTS        { p = 1; next }
p                 { print }
p && $1 == "run:" { exit }
'
}

### get_val bar foo=1,bar=2,baz=3  ==>  2
get_val() {
    [ $# -eq 2 ] || die 'Wrong usage of get_val'
    local KEY="$1"
    local OPTS="$2"

    echo "$OPTS" | tr , \\n | sed -n "s/^$KEY=//p"
}

### Generate CSV file.
gen_csv() {
    [ $# -ge 5 ] || die 'Wrong usage of gen_csv'
    local OUT="$1"; shift
    local TMP="$1"; shift
    local COMMON_OPTS="$1"; shift
    local VAR_OPT="$1"; shift
    local VALUES="$*"

    local NR_MSGS=$(get_val nr_msgs $COMMON_OPTS)

    echo "# $VAR_OPT time msg/s MB/s" >$OUT

    for x in $VALUES; do
	local OPTS="${VAR_OPT}=${x},${COMMON_OPTS}"
	echo "----------[ $OPTS ]----------"

	rpc-ub -o $OPTS | tee $TMP
	[ ${PIPESTATUS[0]} -eq 0 ] || exit ${PIPESTATUS[0]}

	local NR_CONNS=$(get_val nr_conns $OPTS)
	local MSG_LEN=$(get_val msg_len $OPTS)
	awk -v X=$x -v NR_MSGS=$((NR_CONNS * NR_MSGS)) -v MSG_LEN=$MSG_LEN \
	    -v PROG=${0##*/} '
function die(msg) {
	print PROG ": " msg >"/dev/stderr"
	exit 1
}

$1 == "run:" {
	if (runs > 0)
		die("Too many runs")
	time = $6
	mps = NR_MSGS / time
	print X, time, mps, mps * MSG_LEN / 1024 / 1024
	++runs
}

END {
	if (runs == 0)
		die("No data to parse")
}
' $TMP >>$OUT
    done
}

### Generate gnuplot script.
gen_script() {
    [ $# -eq 3 ] || die 'Wrong usage of gen_script'
    local TITLE="$1"
    local CSV="$2"
    local XLABEL="$3"

    cat <<EOF
set terminal png size 480,360

set title '$TITLE'

set xlabel '$XLABEL'
set ylabel 'rate [msg/s]'
set ytics nomirror
set y2tics
set y2label 'rate [MB/s]'

set output '${CSV%.csv}.png'
plot '$CSV' using 1:3 title 'msg/s' axes x1y1 with linespoints, \
         '' using 1:4 title 'MB/s' axes x1y2 with linespoints
EOF
}

TMP=`mktemp -t "${0##*/}.XXXXXXX"`
trap "rm $TMP" 0

i=0
{
    cat <<EOF
msg_len=1024,nr_msgs=100 nr_conns 8 16 32 64 96
nr_conns=32,nr_msgs=400  msg_len  256 512 1024 2048
nr_conns=96,nr_msgs=100  msg_len  64 128 256 512
EOF
} | while read -a ARGS; do
    CSV=$((++i)).csv
    gen_csv $CSV $TMP ${ARGS[@]}
    gen_script ${ARGS[0]} $CSV ${ARGS[1]} >$TMP
    gnuplot $TMP
done
