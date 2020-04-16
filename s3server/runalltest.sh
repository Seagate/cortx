#!/bin/sh
# Script to run UTs & STs
set -e

usage() {
  echo 'Usage: ./runalltest.sh [--no-mero-rpm][--no-ut-run][--no-st-run][--no-ossperf-run][--no-https]
        [--use-ipv6][--basic-s3cmd-rand | --basic-s3cmd-zero]'
  echo '                       [--help]'
  echo 'Optional params as below:'
  echo '          --no-mero-rpm    : Use mero libs from source code (third_party/mero)'
  echo '                             Default is (false) i.e. use mero libs from pre-installed'
  echo '                             mero rpm location (/usr/lib64)'
  echo '          --no-ut-run      : Do not run UTs, Default (false)'
  echo '          --no-st-run      : Do not run STs, Default (false)'
  echo '          --no-https       : Use http for STs, Default (false)'
  echo '          --use-ipv6       : Use ipv6 for STs, Default (false)'
  echo '          --no-ossperf-run : Do not run parallel/sequential perf tests by ossperf tool, Default (false)'
  echo '          --basic-s3cmd-rand: Run only basic s3cmd regression tests with random filled objects'
  echo '          --basic-s3cmd-zero: Run only basic s3cmd regression tests with zero filled objects'
  echo '          --help (-h)      : Display help'
}

# read the options
OPTS=`getopt -o h --long no-mero-rpm,no-ut-run,no-st-run,no-https,use-ipv6,no-ossperf-run,basic-s3cmd-rand,basic-s3cmd-zero,help -n 'runalltest.sh' -- "$@"`

eval set -- "$OPTS"

no_mero_rpm=0
no_ut_run=0
no_st_run=0
use_http=0
no_ossperf_run=0
use_ipv6=0

basic_only_rand=0
basic_only_zero=0
cont_zero_param=""

# extract options and their arguments into variables.
while true; do
  case "$1" in
    --no-mero-rpm) no_mero_rpm=1; shift ;;
    --no-ut-run) no_ut_run=1; shift ;;
    --no-st-run) no_st_run=1; shift ;;
    --no-https) use_http=1; shift ;;
    --use-ipv6) use_ipv6=1; shift ;;
    --no-ossperf-run) no_ossperf_run=1; shift ;;
    --basic-s3cmd-rand) basic_only_rand=1; shift ;;
    --basic-s3cmd-zero) basic_only_zero=1; cont_zero_param="--cont_zero"; shift ;;
    -h|--help) usage; exit 0;;
    --) shift; break ;;
    *) echo "Internal error! $1" ; exit 1 ;;
  esac
done

if [ $basic_only_zero -eq 1 ] && [ $basic_only_rand -eq 1 ]
then
    echo "Only one basic s3cmd test configuration should be specified"
    exit 1
fi

abort()
{
    echo >&2 '
***************
*** ABORTED ***
***************
'
    echo "Error encountered. Exiting test runs prematurely..." >&2
    trap : 0
    exit 1
}
trap 'abort' 0

if [ $no_mero_rpm -eq 1 ]
then
# use mero libs from source code
export LD_LIBRARY_PATH="$(pwd)/third_party/mero/mero/.libs/:"\
"$(pwd)/third_party/mero/helpers/.libs/:"\
"$(pwd)/third_party/mero/extra-libs/gf-complete/src/.libs/"
fi

WORKING_DIR=`pwd`

if [ $basic_only_zero -eq 1 ] || [ $basic_only_rand -eq 1 ]
then
    CLITEST_SRC=`pwd`/st/clitests
    cd $CLITEST_SRC

    sh ./s3cmd_basic_reg_test.sh --config ./pathstyle.s3cfg $cont_zero_param

    cd $WORKING_DIR
    trap : 0
    exit 0
fi

if [ $no_ut_run -eq 0 ]
then
  UT_BIN=`pwd`/bazel-bin/s3ut
  UT_DEATHTESTS_BIN=`pwd`/bazel-bin/s3utdeathtests
  UT_MEMPOOL_BIN=`pwd`/bazel-bin/s3mempoolut
  UT_MEMPOOLMGR_BIN=`pwd`/bazel-bin/s3mempoolmgrut
  UT_S3BACKGROUNDDELETE=`pwd`/s3backgrounddelete/scripts/run_all_ut.sh

  printf "\nCheck s3ut..."
  type  $UT_BIN >/dev/null
  printf "OK \n"

  $UT_BIN 2>&1

  printf "\nCheck s3mempoolmgrut..."
  type  $UT_MEMPOOLMGR_BIN >/dev/null
  printf "OK \n"

  $UT_MEMPOOLMGR_BIN 2>&1

  printf "\nCheck s3utdeathtests..."
  type  $UT_DEATHTESTS_BIN >/dev/null
  printf "OK \n"

  $UT_DEATHTESTS_BIN 2>&1

  printf "\nCheck s3mempoolut..."
  type $UT_MEMPOOL_BIN >/dev/null
  printf "OK \n"

  $UT_MEMPOOL_BIN 2>&1

  printf "\nCheck s3backgrounddeleteut..."
  type $UT_S3BACKGROUNDDELETE >/dev/null
  printf "OK \n"

  $UT_S3BACKGROUNDDELETE 2>&1
fi

if [ $no_st_run -eq 0 ]
then
  use_ipv6_arg=""
  if [ $use_ipv6 -eq 1 ]
  then
    use_ipv6_arg="--use_ipv6"
  fi

  CLITEST_SRC=`pwd`/st/clitests
  cd $CLITEST_SRC
  if [ $use_http -eq 1 ]
  then
     sh ./runallsystest.sh --no_https $use_ipv6_arg
  else
     sh ./runallsystest.sh $use_ipv6_arg
  fi
fi

if [ $no_ossperf_run -eq 0 ]
then
  PERF_SRC=$WORKING_DIR/perf
  cd $PERF_SRC

  if [ -d "testfiles" ]
  then
    rm -rf testfiles
  fi

  echo "ossperf tests..."
  echo "Parallel worload of 5 files each of size 5000 bytes"
  USE_HTTP_FLAG=""
  if [ $use_http -eq 1 ] ; then
    USE_HTTP_FLAG=" -x "
  fi

  ossperf.sh -n 5 -s 5000 -b seagatebucket -c ../st/clitests/virtualhoststyle.s3cfg -p $USE_HTTP_FLAG 2>&1

  if [ $? -ne 0 ]; then
    echo "ossperf -- parallel workload test succeeded"
  fi
  echo "Sequential workload of 5 files each of size 5000 bytes"
  ossperf.sh -n 5 -s 5000 -b seagatebucket -c ../st/clitests/virtualhoststyle.s3cfg $USE_HTTP_FLAG 2>&1

  if [ $? -ne 0 ]; then
     echo "ossperf -- sequential workload test succeeded"
  fi
  # Parallel multipart workload
  ossperf.sh -n 2 -s 18874368 -b seagatebucket -c ../st/clitests/virtualhoststyle.s3cfg -p $USE_HTTP_FLAG 2>&1

  if [ $? -ne 0 ]; then
     echo "ossperf --  parallel workload(Multipart) succeeded"
  fi
  # Sequential multipart workload
  ossperf.sh -n 2 -s 18874368 -b seagatebucket -c ../st/clitests/virtualhoststyle.s3cfg $USE_HTTP_FLAG 2>&1

  if [ $? -ne 0 ]; then
     echo "ossperf -- sequential workload(Multipart) succeeded"
  fi
echo "*************************************************"
echo "*** System tests with ossperf Runs Successful ***"
echo "*************************************************"

fi

cd $WORKING_DIR
trap : 0
