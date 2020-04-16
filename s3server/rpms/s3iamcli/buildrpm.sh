#!/bin/sh

set -xe

SCRIPT_PATH=$(readlink -f "$0")
BASEDIR=$(dirname "$SCRIPT_PATH")
OS=$(cat /etc/os-release | grep -w ID | cut -d '=' -f 2)
VERSION=$(cat /etc/os-release | grep -w VERSION_ID | cut -d '=' -f 2)

GIT_VER=
S3IAMCLI_VERSION=1.0.0
VER_PATH_EXCL=0
PATH_SRC=""

usage() { echo "Usage: $0 (-G <git short revision> | -P <path to sources>) [-S <S3 IAM CLI version>]" 1>&2; exit 1; }

while getopts ":G:S:P:" o; do
    case "${o}" in
        G)
            GIT_VER=${OPTARG}
            [ -z "${GIT_VER}" ] && (echo "Git revision cannot be empty"; usage)
            [ $VER_PATH_EXCL == 1 ] && (echo "Only one of -G or -P can be specified"; usage)
            VER_PATH_EXCL=1
            ;;
        S)
            S3IAMCLI_VERSION=${OPTARG}
            ;;
        P)
            PATH_SRC=$(realpath ${OPTARG})
            [ -d "${PATH_SRC}" ] || (echo "Folder path does not exist"; usage)
            [ $VER_PATH_EXCL == 1 ] && (echo "Only one of -G or -P can be specified"; usage)
            VER_PATH_EXCL=1
            ;;

        *)
            usage
            ;;
    esac
done
shift $((OPTIND-1))

[ $VER_PATH_EXCL == 0 ] && (echo "At least one of -G or -P should be specified"; usage)

echo "Using [S3IAMCLI_VERSION=${S3IAMCLI_VERSION}] ..."
echo "Using [GIT_VER=${GIT_VER}] ..."

mkdir -p ~/rpmbuild/SOURCES/
cd ~/rpmbuild/SOURCES/
rm -rf eos-s3iamcli*
if ! [ -z "${GIT_VER}" ]; then
  # Setup the source tar for rpm build
  git clone http://gerrit.mero.colo.seagate.com:8080/s3server eos-s3iamcli-${S3IAMCLI_VERSION}-git${GIT_VER}
  cd eos-s3iamcli-${S3IAMCLI_VERSION}-git${GIT_VER}
  # For sake of test, attempt checkout of version
  git checkout ${GIT_VER}
elif ! [ -z "${PATH_SRC}" ]; then
    GIT_VER=$(git --git-dir "${PATH_SRC}"/.git rev-parse --short HEAD)
    mkdir -p eos-s3iamcli-${S3IAMCLI_VERSION}-git${GIT_VER}
    cp -ar "${PATH_SRC}"/. ./eos-s3iamcli-${S3IAMCLI_VERSION}-git${GIT_VER}
    cd eos-s3iamcli-${S3IAMCLI_VERSION}-git${GIT_VER}
fi

cd auth-utils
mv s3iamcli eos-s3iamcli-${S3IAMCLI_VERSION}-git${GIT_VER}
tar -zcvf eos-s3iamcli-${S3IAMCLI_VERSION}-git${GIT_VER}.tar.gz eos-s3iamcli-${S3IAMCLI_VERSION}-git${GIT_VER}

cp eos-s3iamcli-${S3IAMCLI_VERSION}-git${GIT_VER}.tar.gz ~/rpmbuild/SOURCES/
cd ~/rpmbuild/SOURCES/
rm -rf eos-s3iamcli-${S3IAMCLI_VERSION}-git${GIT_VER}

extra_defines=()
if [ $VERSION = "\"8.0\"" ]; then
  extra_defines=(--define "s3_with_python36_ver8 1")
fi
yum-builddep -y ${BASEDIR}/s3iamcli.spec "${extra_defines[@]}"
rpmbuild -ba --define "_s3iamcli_version ${S3IAMCLI_VERSION}"  --define "_s3iamcli_git_ver git${GIT_VER}" "${extra_defines[@]}" ${BASEDIR}/s3iamcli.spec --with python3
