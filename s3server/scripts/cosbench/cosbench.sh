#!/bin/sh
set -e
SHORT=h
LONG=help,controller:,drivers:
COSBENCH_VERSION="0.4.2.c3"
configure_action=0
stop_action=0
start_action=0
install_action=0
USERNAME="$(whoami)"

usage() {
  echo ""
  echo 'Usage:'
  echo './cosbench.sh install | configure | start | stop | --help'
  echo '     --controller <controller-ip> --drivers <path to driver-nodes.list file>'
  echo ""
}

OPTS=$(getopt --options $SHORT --long $LONG --name "$0" -- "$@")

eval set -- "$OPTS"

precheck() {
  function version3 { echo "$@" | gawk -F. '{ printf("%03d%03d%03d\n", $1,$2,$3); }'; }
  if command -v curl >/dev/null 2>&1; then
    curl_version=`curl --version | head -n1 | cut -d" " -f2`
    if [ "$(version3 "$curl_version")" -ge "$(version3 "7.22.0")" ]
    then
      printf "\nCheck curl...OK\n"
    else
      printf "\ncurl version to be greater than 7.22.0\n"
      exit 1
    fi
  else
    printf "\ncurl not installed\n"
  fi

  if command -v java >/dev/null 2>&1; then
    JAVA_VER=$(java -version 2>&1 | sed -n ';s/.* version "\(.*\)\.\(.*\)\..*"/\1\2/p;')
    if [ "$JAVA_VER" -ge 16 ]
    then
      printf "\nCheck java OK...\n"
    else
      printf "\nJava Runtime Environment 1.6 or above needed\n"
      exit 1
    fi
  else
    printf "\nJava not installed, please install Java Runtime Environment 1.6 or above\n"
    exit 1
  fi

  if command -v nc >/dev/null 2>&1; then
    printf "\nCheck nc OK...\n"
  else
    printf "\nInstall nc\n"
    exit 1
  fi
}

#make use of COSBENCH_VERSION here -- TODO
install_cmds() {
  cd ~
  rm -f cos
  rm -rf  0.4.2.c3
  wget https://github.com/intel-cloud/cosbench/releases/download/v0.4.2.c3/0.4.2.c3.zip
  unzip 0.4.2.c3.zip
  ln -s 0.4.2.c3 cos
  cd cos
  chmod +x *.sh
}

precheck_on_controller_drivers() {
  printf "\nChecking prerequiste on controller node $CONTROLLER\n"
  ssh "$USERNAME"@"$CONTROLLER" "$(typeset -f precheck); precheck"
  for driver_host in `cat $DRIVERS_FILE`
  do
     printf "\nChecking prerequiste on node $driver_host...\n"
     ssh "$USERNAME"@"$driver_host" "$(typeset -f precheck); precheck"
  done
}

install_on_controller_drivers() {
   ssh "$USERNAME"@"$CONTROLLER" "$(typeset -f install_cmds); install_cmds"
   # Install on each driver nodes
   for driver_host in `cat $DRIVERS_FILE`
   do
     echo '\nInstalling cosbench version $COSBENCH_VERSION to driver node $driver_host\n'
     ssh "$USERNAME"@"$driver_host" "$(typeset -f install_cmds); install_cmds"
   done
}

#This function takes driver node name and the node count as argument
configure_driver() {
  DRIVER_STR="[driver]\nname = Driver$2\nurl = http://$1:18088/driver"
  # Create driver.conf file
ssh "$USERNAME"@"$1" <<ENDSSH
  printf "$DRIVER_STR" > ~/cos/conf/driver.conf
  sudo firewall-cmd --zone=public --add-port=18088/tcp --permanent
  sudo firewall-cmd --reload
ENDSSH
}

configure_cosbench() {
  DRIVERS_COUNT=`cat $DRIVERS_FILE | wc -l`
  CONTROLLER_STR="[controller]\nconcurrency = $DRIVERS_COUNT\ndrivers = $DRIVERS_COUNT\nlog_level = INFO\nlog_file = log/system.log\narchive_dir = archive\n\n"
  count=0
  for driver_host in `cat $DRIVERS_FILE`
  do
    count=$((count+1))
    CONTROLLER_STR+="[driver$count]\nname = Driver$count\n"
    CONTROLLER_STR+="url = http://$driver_host:18088/driver\n\n"
    configure_driver $driver_host $count
  done
  # Create controller.conf file
ssh "$USERNAME"@"$CONTROLLER" <<ENDSSH
  printf "$CONTROLLER_STR" > ~/cos/conf/controller.conf
  sudo firewall-cmd --zone=public --add-port=19088/tcp --permanent
  sudo firewall-cmd --reload
ENDSSH
}

start_cosbench_controller_cmds() {
  cd ~/cos
  ./stop-controller.sh
  sleep 15
  cd ~/cos
  ./start-controller.sh
}

start_cosbench_driver_cmds() {
  cd ~/cos
  ./stop-driver.sh
  sleep 15
  cd ~/cos
  ./start-driver.sh
}

stop_cosbench_controller_cmds() {
  cd ~/cos
  ./stop-controller.sh
}

stop_cosbench_driver_cmds() {
  cd ~/cos
  ./stop-driver.sh
}

start_cosbench() {
  #Start controller
  printf "Starting Controller on node $CONTROLLER\n"
  ssh "$USERNAME"@"$CONTROLLER" "$(typeset -f start_cosbench_controller_cmds); start_cosbench_controller_cmds"

  for driver_host in `cat $DRIVERS_FILE`
  do
    printf "Starting driver on $driver_host...\n"
    ssh "$USERNAME"@"$driver_host" "$(typeset -f start_cosbench_driver_cmds); start_cosbench_driver_cmds"
  done
}

stop_cosbench() {
  ssh "$USERNAME"@"$CONTROLLER" "$(typeset -f stop_cosbench_controller_cmds); stop_cosbench_controller_cmds"

  for driver_host in `cat $DRIVERS_FILE`
  do
    printf "Stopping driver on $driver_host...\n"
    # Check if driver_host is  present in driver.conf, if not give error message
    ssh "$USERNAME"@"$driver_host" "$(typeset -f stop_cosbench_driver_cmds); stop_cosbench_driver_cmds"
  done
}

invalid_command() {
  printf "Invalid command\n"
  usage
  exit 1
}

# set initial values
VERBOSE=false
# extract options and their arguments into variables.
while true ; do
  case "$1" in
    -h | --help )
      usage
      exit 0
      ;;
    --controller )
      CONTROLLER="$2"
      shift 2
      ;;
    --drivers )
      DRIVERS_FILE="$2"
      shift 2
      ;;
    -- )
      shift
      ;;
    install )
      ACTION="$1"
      if [ $configure_action -eq 1 ] || [ $stop_action -eq 1 ] || [ $start_action -eq 1 ]
      then
        invalid_command
      fi
      install_action=1
      shift
      break
      ;;
    configure )
      ACTION="$1"
      if [ $install_action -eq 1 ] || [ $stop_action -eq 1 ] || [ $start_action -eq 1 ]
      then
        invalid_command
      fi
      configure_action=1
      shift
      break
      ;;
    stop )
      ACTION="$1"
      if [ $install_action -eq 1 ] || [ $configure_action -eq 1 ] || [ $start_action -eq 1 ]
      then
        invalid_command
      fi
      stop_action=1
      shift
      break
      ;;
    start )
      ACTION="$1"
      if [ $install_action -eq 1 ] || [ $configure_action -eq 1 ] || [ $stop_action -eq 1 ]
      then
        invalid_command
      fi
      start_action=1
      shift
      break
      ;;
    *)
      invalid_command
      ;;
  esac
done
# Print the variables
if [ $ACTION = "install" ]
then
  precheck_on_controller_drivers
  install_on_controller_drivers
  printf "\n Cosbench installation completed\n"
elif [ $ACTION = "configure" ]
then
  configure_cosbench
  printf "\n Cosbench configuration completed\n"
elif [ $ACTION = "start" ]
then
  start_cosbench
  printf "\n Cosbench started\n"
else
  stop_cosbench
  printf "\n Cosbench stopped\n"
fi
