#!/bin/sh

USAGE="USAGE: $(basename "$0") [-I | -R | -S | -y <configure-yum-repo> | -p <path-to-prod-cfg> | -U | -D]

where:
-I	Install mero, mero-devel, halon, s3iamcli from rpms
	s3server will be built from local sources
-R	Remove mero, mero-devel, halon, s3iamcli, s3server, s3server-debuginfo packages
-S	Show status
-y <configure-yum-repo>	Configure yum repo to install mero and halon from
                       	'hermi' - hermi repos
                       	'<sprint>' - i.e. 'ees1.0.0-PI.1-sprint4' repos
-p <path-to-prod-cfg>  	Use path as s3server config
                       	@ - for s3server/s3config.release.yaml config
-U	Up - start cluster
-D	Down - stop cluster

Operations could be combined into single command, i.e.

$(basename "$0") -y ees1.0.0-PI.1-sprint4 -RI -p @ -US

the command will update repos, remove pkgs, install pkgs, update s3server config,
run cluster and print statuses of all installed pkgs
"

usage() {
    echo "$USAGE" 1>&2;
    exit 1
}

set -e

SCRIPT_PATH=$(readlink -f "$0")
BASEDIR=$(dirname "$SCRIPT_PATH")

USE_SUDO=
if [[ $EUID -ne 0 ]]; then
  command -v sudo || (echo "Script should be run as root or sudo required." && exit 1)
  USE_SUDO=sudo
fi

install_pkgs() {
    $USE_SUDO yum -t -y install mero
    $USE_SUDO yum -t -y install mero-devel
    $USE_SUDO yum -t -y install halon

    # option "-P" will build s3server rpm from the source tree
    # option "-i" will install s3server from the built rpm
    $USE_SUDO ${BASEDIR}/rpms/s3/buildrpm.sh -P ${BASEDIR} -i
    $USE_SUDO yum -t -y install s3iamcli
}

remove_pkgs() {
    $USE_SUDO yum -t -y remove s3iamcli s3server-debuginfo s3server halon mero-devel mero 
}

sysctl_stat() {
    $USE_SUDO systemctl status $1 &> /dev/null
    local st=$?
    case $st in
        0) echo "running" ;;
        3) echo "stopped" ;;
        *) echo "error" ;;
    esac
}

haproxy_ka_status() {
    $USE_SUDO grep -e "^\s*#\s*option httpchk HEAD" /etc/haproxy/haproxy.cfg &> /dev/null && echo "disabled"
    $USE_SUDO grep -e "^\s*option httpchk HEAD" /etc/haproxy/haproxy.cfg &> /dev/null && echo "enabled"
    $USE_SUDO grep -e "option httpchk HEAD" /etc/haproxy/haproxy.cfg &> /dev/null || echo "option not found"
}

status_srv() {
    case "$1" in
        s3server) echo -e "\t\t PIDs:> $(pgrep $1 | tr '\n' ' ')"
                  echo -e "\t\t s3authserver:> $(sysctl_stat s3authserver)"
                  echo -e "\t\t $($USE_SUDO grep -o -e "S3_LOG_MODE:\s*\S*" /opt/seagate/s3/conf/s3config.yaml)"
                  echo -e "\t\t $($USE_SUDO grep -o -e "S3_LOG_ENABLE_BUFFERING:\s*\S*" /opt/seagate/s3/conf/s3config.yaml)"
                  ;;
        haproxy) echo -e "\t\t $(sysctl_stat $1)"
                 echo -e "\t\t keepalive $(haproxy_ka_status)"
                 ;;
        halon) echo -e "\t\t halond:> $(sysctl_stat halond)"
               ;;
        openldap) echo -e "\t\t $(sysctl_stat slapd)"
                  ;;
        mero) $USE_SUDO hctl mero status
              ;;
        *)
              ;;
    esac
}

status_pkgs() {
    for test_pkg in s3server-debuginfo s3server halon mero-devel haproxy openldap mero
    do
        set +e
        $USE_SUDO yum list installed $test_pkg &> /dev/null
        local inst_stat=$?
        echo "Package $test_pkg"
        [ $inst_stat == 0 ] && echo -e "\t\t installed" || echo -e "\t\t not found"
        [ $inst_stat == 0 ] && status_srv $test_pkg
        set -e
    done
}

yum_repo_conf() {
    case "$1" in
        hermi) $USE_SUDO rm -f /etc/yum.repos.d/releases_sprint.repo || true
               for f in /etc/yum.repos.d/*hermi*repo; do $USE_SUDO sed -i 's/priority.*/priority = 1/' $f; done
               for f in /etc/yum.repos.d/*s3server*repo; do $USE_SUDO sed -i 's/priority.*/priority = 1/' $f; done
               ;;
        ees*-sprint*) echo "Use sprint $1 builds"
                      $USE_SUDO echo "[sprints_s3server]
baseurl = http://ci-storage.mero.colo.seagate.com/releases/eos/${1}/s3server/repo
gpgcheck = 0
name = Yum repo for s3server sprints build
priority = 1

[sprints_halon]
baseurl = http://ci-storage.mero.colo.seagate.com/releases/eos/${1}/halon/repo
gpgcheck = 0
name = Yum repo for halon sprints build
priority = 1

[sprints_mero]
baseurl = http://ci-storage.mero.colo.seagate.com/releases/eos/${1}/mero/repo
gpgcheck = 0
name = Yum repo for mero sprints build
priority = 1
" > /etc/yum.repos.d/releases_sprint.repo
                      for f in /etc/yum.repos.d/*hermi*repo; do $USE_SUDO sed -i 's/priority.*/priority = 2/' $f; done
                      for f in /etc/yum.repos.d/*s3server*repo; do $USE_SUDO sed -i 's/priority.*/priority = 2/' $f; done
                      ;;
        *) echo "Invalid sprint name provided"
           ;;
    esac
    $USE_SUDO yum clean all
    $USE_SUDO  rm -rf /var/cache/yum
}

up_cluster() {
    $USE_SUDO ./scripts/enc_ldap_passwd_in_cfg.sh -l ldapadmin -p /opt/seagate/auth/resources/authserver.properties
    $USE_SUDO systemctl start haproxy
    $USE_SUDO systemctl start slapd
    $USE_SUDO systemctl start s3authserver
    $USE_SUDO rm -fR /var/mero/*
    $USE_SUDO m0setup -P 1 -N 1 -K 0 -vH
    $USE_SUDO sed -i 's/- name: m0t1fs/- name: s3server/' /etc/halon/halon_facts.yaml
    $USE_SUDO awk 'BEGIN {in_section=0} {if ($0 ~ /^- name:/) {in_section=0; if ($0 ~ /^- name: "s3server"/) {in_section=1;} } {if (in_section==1) {gsub("multiplicity: 4", "multiplicity: 2");} print;} }' /etc/halon/mero_role_mappings > /tmp/mero_role_mappings
    $USE_SUDO cp /tmp/mero_role_mappings /etc/halon/mero_role_mappings
    $USE_SUDO rm /tmp/mero_role_mappings
    $USE_SUDO systemctl start halon-cleanup
    $USE_SUDO systemctl start halond
    $USE_SUDO hctl mero bootstrap
    sleep 2
    $USE_SUDO hctl mero status
}

down_cluster() {
    $USE_SUDO hctl mero stop
    $USE_SUDO systemctl stop halond
    $USE_SUDO systemctl stop halon-cleanup
    $USE_SUDO systemctl stop s3authserver
    $USE_SUDO systemctl stop slapd
    $USE_SUDO systemctl stop haproxy
}

while getopts ":IRSy:p:UD" o; do
    case "${o}" in
        I) echo "Installing..."
           install_pkgs
           ;;
        R) echo "Removing..."
           remove_pkgs
           ;;
        S) echo "Status..."
           status_pkgs
           ;;
        y) echo "Confirure yum repos to use <${OPTARG}>..."
           yum_repo_conf ${OPTARG}
           ;;
        p) copy_path=${OPTARG}
           if [ "$copy_path" == "@" ]; then
               copy_path=${BASEDIR}/s3config.release.yaml
           fi
           echo "Copy config ${copy_path} to /opt/seagate/s3/conf/s3config.yaml"
           $USE_SUDO mkdir -p /opt/seagate/s3/conf/
           [ -f /opt/seagate/s3/conf/s3config.yaml ] && $USE_SUDO cp -f /opt/seagate/s3/conf/s3config.yaml /opt/seagate/s3/conf/s3config.yaml_bak
           $USE_SUDO cp "${copy_path}" /opt/seagate/s3/conf/s3config.yaml
           ;;
        U) echo "Up"
           up_cluster
           ;;
        D) echo "Down"
           down_cluster
           ;;
        *) usage
           ;;
    esac
done
shift $((OPTIND-1))
