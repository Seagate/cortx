#!/bin/bash

# Runs FIO test suite.

die() {
  echo "${*}"
  exit 1
}

print_usage_and_exit() {
  progname=runall.sh
  cat <<EOF
Usage:
  $progname -h|-help|--help
  $progname run_time [-min] mpatha mpathb ... mpathlast

run_time is time to run in seconds.

When -min is specified, instead of full suite script will only run basic seq
read and seq write tests.

To find which mpathX are available on your system, run:

lsblk

or

multipath -ll

Full path are located in /dev/disk/by-id/dm-name-mpath*.

NOTE: make sure you do not specify the volume which is holding /var/mero! check
lsblk output, it shows which one is used.

Example command line for quick test:

$progname 20 mpathi mpatha{k..p}

EOF
  die "${@}"
}

if [ "$1" = -h -o "$1" = --help -o "$1" = -help ]; then
  print_usage_and_exit
fi

run_time="$1"
shift

if ! [[ $run_time =~ ^[1-9][0-9]*$ ]]; then
  print_usage_and_exit "run_time specifed as '$run_time'. must be an integer, valid for fio 'runtime' parameter."
fi

min_test=false
if [ "$1" = '-min' ]; then
  min_test=true
  shift
fi

mpaths=("${@}")

fullpath() {
  echo "/dev/disk/by-id/dm-name-$1"
}

for mpath in "${mpaths[@]}"; do
  fp=`fullpath "$mpath"`
  if ! test -b "$fp"; then
    print_usage_and_exit "Device $mpath is not found at $fp"
  fi
done

echo "Running FIO test suite with runtime = $run_time and the following volumes:"
for mpath in "${mpaths[@]}"; do
  echo "  $mpath -> "`fullpath "$mpath"`
done

set -e

fio_dir="$(dirname "$BASH_SOURCE")"
test -d "$fio_dir" || die "FIO dir not found <$fio_dir>"

outputs_dir=`pwd`

test -f "runall.sh" && die "Current folder already contains results, create new directory and run from there."

mkdir "$outputs_dir/test_suite"

cd "$fio_dir/fio-configs"
if $min_test; then
  fio_configs=(7volumes.rw-read.numjobs-1.bs-2m.fio 7volumes.rw-write.numjobs-1.bs-2m.fio)
else
  fio_configs=(7volumes.*.fio)
fi
for i in "${fio_configs[@]}"; do
  cd "$outputs_dir"
  echo "Running $i."
  config="$outputs_dir/test_suite/$i"
  cp "$fio_dir/fio-configs/$i" "$config"
  sed -i "s/runtime=.*/runtime=$run_time/" "$config"
  for mpath in "${mpaths[@]}"; do
    fp=`fullpath "$mpath"`
    echo -e "\n[$mpath]\nfilename=$fp" >> "$config"
  done
  fio "$config" &>"$outputs_dir/$i-output"
done

cp "$fio_dir/runall.sh" "$outputs_dir"
cp "$fio_dir/baseline.sh" "$outputs_dir"

cd "$outputs_dir"
./baseline.sh
