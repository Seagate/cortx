#!/bin/sh
set -e
SHORT=h
LONG=help,controller:,list_running,cancel:
CONTROLLER=""
USERNAME="$(whoami)"

usage() {
  echo 'Usage:'
  echo '  ./manage-workload-status.sh --list_running | --cancel <workload id>'
  echo '       --controller <controller-ip> [--help]'
}

OPTS=$(getopt --options $SHORT --long $LONG  --name "$0" -- "$@")

eval set -- "$OPTS"

invalid_command() {
  printf "\nInvalid command\n"
  usage
  exit 1
}

while true ; do
  case "$1" in
    --controller )
      CONTROLLER="$2"
      shift 2
      ;;
    -h | --help )
      usage
      exit 0
      ;;
    --list_running )
      if [ "$ACTION" == "--cancel" ]
      then
        invalid_command
      fi
      ACTION="$1"
      shift 1
      ;;
    --cancel )
      if [ "$ACTION" == "--list_running" ]
      then
         invalid_command
      fi
      ACTION="$1"
      WORKLOAD_ID="$2"
      shift 2
      ;;
    -- )
      shift
      break
      ;;
    *)
      printf "$1\n"
      invalid_command
      ;;
  esac
done

if [ "$ACTION" == "--list_running" ]
then
  result=$(ssh "$USERNAME"@"$CONTROLLER" "cd ~/cos; sh cli.sh info")
  echo "$result"
fi

if [ "$ACTION" == "--cancel" ]
then
  result=$(ssh "$USERNAME"@"$CONTROLLER" "cd ~/cos; sh cli.sh cancel "$WORKLOAD_ID"")
  echo "$result"
fi
