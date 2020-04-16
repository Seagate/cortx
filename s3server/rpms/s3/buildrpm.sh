#!/bin/sh

set -e

SCRIPT_PATH=$(readlink -f "$0")
BASEDIR=$(dirname "$SCRIPT_PATH")

GIT_VER=
S3_VERSION=1.0.0
PATH_SRC=""
VER_PATH_EXCL=0
INSTALL_AFTER_BUILD=0

usage() { echo "Usage: $0 [-S <S3 version>] [-i] (-G <git short revision> | -P <path to sources>)" 1>&2; exit 1; }

while getopts ":G:S:P:i" o; do
    case "${o}" in
        G)
            GIT_VER=${OPTARG}
            [ -z "${GIT_VER}" ] && (echo "Git revision cannot be empty"; usage)
            [ $VER_PATH_EXCL == 1 ] && (echo "Only one of -G or -P can be specified"; usage)
            VER_PATH_EXCL=1
            ;;
        S)
            S3_VERSION=${OPTARG}
            ;;
        P)
            PATH_SRC=$(realpath ${OPTARG})
            [ -d "${PATH_SRC}" ] || (echo "Folder path does not exist"; usage)
            [ $VER_PATH_EXCL == 1 ] && (echo "Only one of -G or -P can be specified"; usage)
            VER_PATH_EXCL=1
            ;;
        i)
            INSTALL_AFTER_BUILD=1
            ;;
        *)
            usage
            ;;
    esac
done
shift $((OPTIND-1))

[ $VER_PATH_EXCL == 0 ] && (echo "At least one of -G or -P should be specified"; usage)

echo "Using [S3_VERSION=${S3_VERSION}] ..."
! [ -z "${GIT_VER}" ] && echo "Using [GIT_VER=${GIT_VER}] ..."
! [ -z "${PATH_SRC}" ] && echo "Using [PATH_SRC=${PATH_SRC}] ..."
echo "Install after build ${INSTALL_AFTER_BUILD}"

set -xe

mkdir -p ~/rpmbuild/SOURCES/
cd ~/rpmbuild/SOURCES/
rm -rf eos-s3server*

if ! [ -z "${GIT_VER}" ]; then
    # Setup the source tar for rpm build
    git clone http://gerrit.mero.colo.seagate.com:8080/s3server eos-s3server-${S3_VERSION}-git${GIT_VER}
    cd eos-s3server-${S3_VERSION}-git${GIT_VER}
    # For sake of test, attempt checkout of version
    git checkout ${GIT_VER}
elif ! [ -z "${PATH_SRC}" ]; then
    GIT_VER=$(git --git-dir "${PATH_SRC}"/.git rev-parse --short HEAD)
    mkdir -p eos-s3server-${S3_VERSION}-git${GIT_VER}
    cp -ar "${PATH_SRC}"/. ./eos-s3server-${S3_VERSION}-git${GIT_VER}
    find ./eos-s3server-${S3_VERSION}-git${GIT_VER} -type f -name CMakeCache.txt -delete;
fi

cd ~/rpmbuild/SOURCES/
tar -zcvf eos-s3server-${S3_VERSION}-git${GIT_VER}.tar.gz eos-s3server-${S3_VERSION}-git${GIT_VER}
rm -rf eos-s3server-${S3_VERSION}-git${GIT_VER}

cd ~/rpmbuild/SOURCES/

yum-builddep -y ${BASEDIR}/s3rpm.spec

rpmbuild -ba \
         --define "_s3_version ${S3_VERSION}" \
         --define "_s3_git_ver git${GIT_VER}" \
         ${BASEDIR}/s3rpm.spec --with python3

if [ $INSTALL_AFTER_BUILD == 1 ]; then
    RPM_ARCH=$(rpm --eval "%{_arch}")
    RPM_DIST=$(rpm --eval "%{?dist:el7}")
    RPM_BUILD_VER=$(test -n "$build_number" && echo "$build_number" || echo 1 )
    RPM_PATH=~/rpmbuild/RPMS/${RPM_ARCH}/eos-s3server-${S3_VERSION}-${RPM_BUILD_VER}_git${GIT_VER}_${RPM_DIST}.${RPM_ARCH}.rpm
    echo "Installing $RPM_PATH..."
    rpm -i $RPM_PATH
fi
