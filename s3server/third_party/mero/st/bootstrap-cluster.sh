#!/usr/bin/env bash
set -eux
# export PS4='+ [${FUNCNAME[0]:+${FUNCNAME[0]}:}${LINENO}] '

die() { echo "$@" >&2; exit 1; }

configure_beta1() {
	CMU_HOST="172.16.0.41"
	HOSTS_LIST="172.16.1.[1-6]"
	CLIENTS_LIST=""
	CLUSTER_TYPE="real"
}

configure_dev1_1() {
	CMU_HOST="172.16.0.41"
	HOSTS_LIST="172.16.1.[3,5,6,8,9]"
	CLIENTS_LIST=""
	CLUSTER_TYPE="real"
}

configure_dev2_1() {
	CMU_HOST="172.16.0.41"
	CLIENTS_LIST="172.18.3.19[1-4]"
	HOSTS_LIST="$CMU_HOST,172.16.1.[1,3,4,6,7],$CLIENTS_LIST"
	CLUSTER_TYPE="real"
}

configure_dev2_2() {
	CMU_HOST="172.16.0.42"
	HOSTS_LIST="$CMU_HOST,172.16.2.[1-7,9-10]"
	CLIENTS_LIST="172.16.2.[9-10]"
	CLUSTER_TYPE="real"
}

configure_dev3() {
	CMU_HOST="172.16.0.41"
	HOSTS_LIST="$CMU_HOST,172.16.1.[1,3-7],172.16.2.[1,2,4]"
	CLIENTS_LIST=""
	CLUSTER_TYPE="real"
}

configure_beta5() {
	CMU_HOST="172.16.0.41"
	HOSTS_LIST="$CMU_HOST,172.16.1.[1-7,18,20]"
	CLIENTS_LIST="172.16.1.[18,20]"
	CLUSTER_TYPE="real"
}

configure_fre7n1() {
	CMU_HOST="172.16.0.41"
	HOSTS_LIST="172.16.1.[1-5]"
	CLIENTS_LIST=""
	CLUSTER_TYPE="kvm"
}

configure_hvt() {
	CMU_HOST="172.16.0.41"
	HOSTS_LIST="$CMU_HOST,172.16.1.[1-6]"
	CLIENTS_LIST="172.16.1.1"
	CLUSTER_TYPE="kvm"
}

configure_hvt_clovis() {
	CMU_HOST="172.16.0.41"
	HOSTS_LIST="$CMU_HOST,172.16.1.[1-6]"
	CLIENTS_LIST="172.16.1.1"
	MERO_ROLE_MAPPINGS="/etc/halon/role_maps/clovis.ede"
	CLUSTER_TYPE="kvm"
}

configure_s3single() {
	CMU_HOST="192.168.0.2"
	HOSTS_LIST="$CMU_HOST,192.168.0.1"
	CLIENTS_LIST=""
	MERO_ROLE_MAPPINGS="/etc/halon/role_maps/s3server.ede"
	CLUSTER_TYPE="real"
}

configure_kvm2dm() {
	CMU_HOST="172.16.0.41"
	HOSTS_LIST="$CMU_HOST,172.16.1.[1-7]"
	CLIENTS_LIST="172.16.1.[1-2]"
	CLUSTER_TYPE="kvm"
}

configure_common() {
	HALON_SOURCES=${HALON_SOURCES:-/root/halon}
	MERO_SOURCES=${MERO_SOURCES:-/root/mero}
	MERO_RPM_PATH=${MERO_RPM_PATH:-/root/rpmbuild/RPMS/x86_64}
	HALON_RPM_PATH=$HALON_SOURCES/rpmbuild/RPMS/x86_64
	REMOTE_RPM_PATH=${REMOTE_RPM_PATH:-/tmp}

	HALON_FACTS_FUNC="${HALON_FACTS_FUNC:-halon_facts_yaml_auto}"
	HALON_FACTS_HOST="$CMU_HOST"
	HALON_FACTS_PATH="/etc/halon/halon_facts.yaml"
	MERO_ROLE_MAPPINGS="${MERO_ROLE_MAPPINGS:-/etc/halon/mero_role_mappings}"
	HALON_ROLE_MAPPINGS="/etc/halon/halon_role_mappings"

	PDSH="pdsh -S -w $HOSTS_LIST"
	PDCP="pdcp -w $HOSTS_LIST"
}

run_command() {
	case "$1" in
	"prepare_build_node")
		prepare_build_node
		;;
	"build_mero")
		build_mero
		;;
	"build_halon")
		build_halon
		;;
	"stop")
		ssh $CMU_HOST hctl mero stop || true
		if [ -n "$CLIENTS_LIST" ]; then
			pdsh -w $CLIENTS_LIST systemctl stop mero-kernel
		fi
		$PDSH systemctl stop mero-kernel
		$PDSH systemctl stop halond
		$PDSH 'kill `pidof halond` `pidof halonctl`'
		sleep 2
		$PDSH systemctl start mero-kernel
		sleep 2
		$PDSH systemctl stop mero-kernel
		;;
	"uninstall")
		$PDSH yum -y remove mero halon
		;;
	"install")
		local MERO_RPM=$(ls -t $MERO_RPM_PATH | grep -m1 -P 'mero-\d\.\d\.\d')
		local HALON_RPM=$(ls -t $HALON_RPM_PATH |
				  grep -m1 'halon-.*devel')
		$PDCP $MERO_RPM_PATH/$MERO_RPM $REMOTE_RPM_PATH
		$PDSH yum -y install $REMOTE_RPM_PATH/$MERO_RPM
		$PDCP $HALON_RPM_PATH/$HALON_RPM $REMOTE_RPM_PATH
		$PDSH yum -y install $REMOTE_RPM_PATH/$HALON_RPM
		;;
	"start_halon")
		$PDSH systemctl status sspl-ll
		if [ "$CLUSTER_TYPE" == "kvm" ]; then
			$PDSH systemctl disable sspl-ll
		fi
		$PDSH uptime
		$PDSH "ps aux | grep halond"
		$PDSH "ps aux | grep m0"
		$PDSH "mount | grep m0"
		$PDSH systemctl is-active halond
		$PDSH systemctl is-enabled halond
		$PDSH systemctl status halond
		$PDSH systemctl stop halond
		$PDSH systemctl disable halond
		sleep 5
		$PDSH systemctl is-active halond
		$PDSH systemctl is-enabled halond
		$PDSH systemctl is-failed halond
		$PDSH rm -rvf /var/lib/halon/halon-persistence
		$PDSH rm -rvf /var/log/halon.decision.log
		$PDSH cat /etc/sysconfig/halond
		$PDSH systemctl start halond
		sleep 5
		$PDSH systemctl status halond
		;;
	"bootstrap")
		${HALON_FACTS_FUNC} | ssh $HALON_FACTS_HOST "cat > halon_facts.yaml"
		# -r 30000000 increases Halon lease timeout.
		# It's a temporary workaround implemented as part of HALON-612.
		ssh $HALON_FACTS_HOST "HALOND_STATION_OPTIONS='-r 30000000' \
					hctl mero bootstrap \
					--facts halon_facts.yaml \
					--roles $MERO_ROLE_MAPPINGS \
					--halonroles $HALON_ROLE_MAPPINGS"
					# --facts $HALON_FACTS_PATH \
		;;
	"status")
		$PDSH systemctl status halond
		ssh $CMU_HOST hctl mero status
		;;
	"halon_facts_yaml")
		${HALON_FACTS_FUNC}
		;;
	"run_m0setup")
		if [ "$HALON_FACTS_FUNC" ==  "halon_facts_yaml_auto" ]; then
			# backup the facts file
			${HALON_FACTS_FUNC} | ssh $HALON_FACTS_HOST "cat > halon_facts.yaml"
		fi
		sudo m0setup -v -P 8 -N 2 -K 1 -i 1 -d /var/mero/img -s 128 -c
		if [ "$HALON_FACTS_FUNC" ==  "halon_facts_yaml_auto" ]; then
			# restore the facts file
			ssh $HALON_FACTS_HOST cp halon_facts.yaml $HALON_FACTS_PATH
		fi
		sudo m0setup -v -P 8 -N 2 -K 1 -i 1 -d /var/mero/img -s 128
		sudo rm -vf /etc/mero/genders
		sudo rm -vf /etc/mero/conf.xc
		sudo rm -vf /etc/mero/disks*.conf
		;;
	*)
		die "Unknown command: $1"
	esac
}

setup() {
	local cluster="$1"
	if [ "$cluster" = guess ]; then
		case `hostname` in
			castor-beta1-cc1.xy01.xyratex.com) cluster=beta1;;
			castor-dev1-1-cc1.xy01.xyratex.com) cluster=dev1_1;;
			castor-dev2-1-cc1.dco.colo.seagate.com) cluster=dev2_1;;
			castor-dev2-2-cc1.xy01.xyratex.com) cluster=dev2_2;;
			castor-dev3-cc1.dco.colo.seagate.com) cluster=dev3;;
			castor-beta5-cc1.dco.colo.seagate.com) cluster=beta5;;
			vmc-rekvm-cc1.xy01.xyratex.com) cluster=fre7n1;;
			vmc-rekvm-hvt-cc1.xy01.xyratex.com) cluster=hvt;;
			vmc-rekvm-2dm-cc1.xy01.xyratex.com) cluster=kvm2dm;;
			*) die 'Cannot deduce cluster name.' \
			       'Use --cluster option.';;
		esac
	fi
	## Keep in sync with `--cluster' documentation in usage().
	case $cluster in
		beta1) configure_beta1;;
		dev1_1) configure_dev1_1;;
		dev2_1) configure_dev2_1;;
		dev2_2) configure_dev2_2;;
		dev3) configure_dev3;;
		beta5) configure_beta5;;
		fre7n1) configure_fre7n1;;
		hvt) configure_hvt;;
		hvt_clovis) configure_hvt_clovis;;
		s3single) configure_s3single;;
		kvm2dm) configure_kvm2dm;;
		*) die "Unsupported cluster: $cluster";;
	esac
	configure_common
}

usage() {
	cat << EOF
Usage: ${0##*/} [OPTION]... [--] COMMAND...

Options:
    -h, --help      Show this help and exit.
    --cluster NAME  Use specific cluster configuration. Supported clusters:
                    beta1, dev1_1, dev2_1, dev2_2, dev3, beta5, fre7n1, hvt, s3single.
		    If this option is missing, the script will try to guess by hostname.

Commands:
    prepare_build_node  Install tools necessary to build Mero and Halon.
    build_mero          Build Mero rpm from sources.
    build_halon         Build Halon rpm from sources. Requires \`build_mero'
                        to be called earlier.
    install             Install the latest Mero and Halon rpms from rpm build
                        directories. Requires \`pdcp' (part of \`pdsh' package)
                        to be available at all cluster nodes.
    uninstall           Uninstall Mero and Halon rpms.
    start_halon         Start TS on the first node and SAT on all nodes.
    bootstrap           halonctl cluster load
    status              halonctl status
    stop                Stop all halond and mero services.
    halon_facts_yaml    Show halon_facts.yaml for the cluster.
    run_m0setup         Run m0setup to create loopback devices.
EOF
}

main() {
	local cluster=guess
	local cmd
	local temp # If we assigned here, getopt error would go unnoticed.
	temp=$(getopt -o h --long help,cluster: -n "@{0##*/}" -- "$@")
	eval set -- "$temp"
	while true; do
		case "$1" in
			-h|--help) usage; exit 0;;
			--cluster) cluster=$2; shift;;
			--) shift; break;;
			*) break;;
		esac
		shift
	done
	if [ $# -eq 0 ]; then
		echo "Command is missing" >&2
		usage >&2
		exit 1
	fi
	setup $cluster
	for cmd in "$@"; do
		run_command $cmd
		shift
	done
}

prepare_build_node() {
	# use the following commands to checkout Mero and Halon before
	# running prepare_build_nodes():
	# git clone --recursive http://es-gerrit.xyus.xyratex.com:8080/mero
	# git clone --recursive https://github.com/seagate-ssg/halon.git
	mero/scripts/install-build-deps
	FPCO=https://s3.amazonaws.com/download.fpcomplete.com/centos/7/fpco.repo
	curl -sSL $FPCO | sudo tee /etc/yum.repos.d/fpco.repo
	sudo yum -y install leveldb-devel libgenders-devel stack pdsh
	ssh $CMU_HOST mco castor puppet --off
	ssh $CMU_HOST 'cd /opt/packages && yumdownloader pdsh pdsh-rcmd-ssh'
	ssh $CMU_HOST createrepo /opt/packages
	$PDSH yum clean all
	$PDSH yum -y install pdsh
}

build_mero() {
        cd $MERO_SOURCES
        git clean -dfx
        ./scripts/install-build-deps
        sh autogen.sh
        # take it from m0d -v after the first rpm build
        ./configure '--build=x86_64-redhat-linux-gnu' '--host=x86_64-redhat-linux-gnu' '--program-prefix=' '--disable-dependency-tracking' '--prefix=/usr' '--exec-prefix=/usr' '--bindir=/usr/bin' '--sbindir=/usr/sbin' '--sysconfdir=/etc' '--datadir=/usr/share' '--includedir=/usr/include' '--libdir=/usr/lib64' '--libexecdir=/usr/libexec' '--localstatedir=/var' '--sharedstatedir=/var/lib' '--mandir=/usr/share/man' '--infodir=/usr/share/info' '--enable-release' '--with-trace-kbuf-size=256' '--with-trace-ubuf-size=64' 'build_alias=x86_64-redhat-linux-gnu' 'host_alias=x86_64-redhat-linux-gnu' 'CFLAGS=-O2 -g -pipe -Wall -Wp,-D_FORTIFY_SOURCE=2 -fexceptions -fstack-protector-strong --param=ssp-buffer-size=4 -grecord-gcc-switches   -m64 -mtune=generic' 'LDFLAGS=-Wl,-z,relro '
        make rpms-notests
        make -j5
}

stack_call() {
	PKG_CONFIG_PATH=$MERO_SOURCES MERO_ROOT=$MERO_SOURCES LD_LIBRARY_PATH=$MERO_SOURCES/mero/.libs stack "$@"
}

build_halon() {
	cd $HALON_SOURCES
	if [ "$(grep $MERO_SOURCES stack.yaml)" == "" ]; then
		sed -i "s:/mero:$MERO_SOURCES:" stack.yaml
	fi
	git clean -dfx
	stack_call setup --no-docker
	stack_call build --no-docker --ghc-options='-g -j4'
	make rpm-dev
}

halon_facts_yaml_auto() {
	ssh $HALON_FACTS_HOST cat $HALON_FACTS_PATH
}

main "$@"
