#!/bin/sh

set -xe

# When run with no args, builds all packages.  Can specify package name as 1st
# arg to build just that package.
OS=$(cat /etc/os-release | grep -w ID | cut -d '=' -f 2)
VERSION=$(cat /etc/os-release | grep -w VERSION_ID | cut -d '=' -f 2)
if [ $VERSION = "\"7\"" ] || [ $VERSION = "\"7.6\"" ]
then
  pkg_list="[ wheel, jmespath, xmltodict, botocore, s3transfer, boto3 ]"
  relevant_pkgs=("wheel" "jmespath" "xmltodict" "botocore" "s3transfer" "boto3")
else
  pkg_list="[ xmltodict, scripttest ]"
  relevant_pkgs=("xmltodict" "scripttest")
fi

USAGE="USAGE: bash $(basename "$0") [--all | --python34 | --python36 ]
                                    [--pkg <module to build>]
    Builds python modules:
    $pkg_list.
where:
--all         default. Build for both Python 3.4 & 3.6
--python34    Build for python 3.4
--python36    Build for python 3.6
--pkg         Package to build. When not specified build all packages."

build_for_all_py_vers=0
build_for_py_34=0
build_for_py_36=0
pkg=""

test -f /usr/bin/python
python_exists=$?

if [ $python_exists -ne 0 ]; then
  test -f /usr/bin/python3
  python3_exist=$?
  if [ $python3_exist -eq 0 ]; then
    alternatives --set python /usr/bin/python3
  fi
fi


if [ $# -eq 0 ]
then
  build_for_all_py_vers=1
else
  while [ "$1" != "" ]; do
    case "$1" in
      --all ) build_for_all_py_vers=1;
          ;;
      --python34 ) build_for_py_34=1;
          if [ $VERSION = "\"8.0\"" ]; then
            echo "ERROR: Invalid argument --python34 with $OS $VERSION"
            exit 1
          fi
          ;;
      --python36 ) build_for_py_36=1;
          ;;
      --pkg )
          if [ "$2" = "wheel" ] || \
             [ "$2" = "jmespath" ] || \
             [ "$2" = "xmltodict" ] || \
             [ "$2" = "botocore" ] || \
             [ "$2" = "s3transfer" ] || \
             [ "$2" = "boto3" ] || \
             [ "$2" = "scripttest" ]
          then
            pkg="$2";
          else
            echo "ERROR: Invalid package specified."
            echo "---------------------------------"
            echo ""
            echo "$USAGE"
            exit 1
          fi
          shift;
          ;;
      --help | -h )
          echo "$USAGE"
          exit 1
          ;;
      * )
          echo "$USAGE"
          exit 1
          ;;
    esac
    shift
  done
fi

need_pkg() {
 if [ "$pkg" = "" ]; then
    if [[ " ${relevant_pkgs[*]} " == *"$1"* ]]; then
      return 0
    else
      return 1
    fi
  else
    test "$pkg" = "$1"
  fi
}

install_python36_deps() {
  yum install -y python36 python36-devel
  # Few packages needed for python36 are not available in our epel mirror.
  # We download directly till we get latest epel.
  yum info python36-six > /dev/null || \
    yum install -y https://mirror.umd.edu/fedora/epel/7/x86_64/Packages/p/python36-six-1.11.0-3.el7.noarch.rpm
  yum info python36-docutils > /dev/null || \
    yum install -y https://mirror.umd.edu/fedora/epel/7/x86_64/Packages/p/python36-docutils-0.14-1.el7.noarch.rpm
  yum info python36-dateutil > /dev/null || \
    yum install -y https://mirror.umd.edu/fedora/epel/7/x86_64/Packages/p/python36-dateutil-2.4.2-5.el7.noarch.rpm
  yum info python36-pbr > /dev/null || \
    yum install -y https://mirror.umd.edu/fedora/epel/7/x86_64/Packages/p/python36-pbr-4.2.0-3.el7.noarch.rpm
  yum info python36-nose > /dev/null || \
    yum install -y https://mirror.umd.edu/fedora/epel/7/x86_64/Packages/p/python36-nose-1.3.7-4.el7.noarch.rpm
  yum info python36-mock > /dev/null || \
    yum install -y https://mirror.umd.edu/fedora/epel/7/x86_64/Packages/p/python36-mock-2.0.0-2.el7.noarch.rpm
}

install_python36_deps_rhel8() {
  for package in python36 python36-devel python3-six python3-docutils python3-dateutil python3-nose python3-mock
  do
    yum info $package > /dev/null || yum install -y $package
  done

  yum info python3-pbr > /dev/null ||
    yum install -y https://mirror.umd.edu/fedora/epel/8/Everything/x86_64/Packages/p/python3-pbr-5.1.2-3.el8.noarch.rpm
}

install_python34_deps() {
  yum install -y python34 python34-devel
}

# Prepare custom options list for rpmbuild & yum-builddep:
rpmbuild_opts=()
yumdep_opts=()

if [[ build_for_all_py_vers -eq 1 ]]; then
  if [ $VERSION = "\"8.0\"" ]; then
    echo "Building packages for python 3/3.6"
    install_python36_deps_rhel8
    rpmbuild_opts=("${rpmbuild_opts[@]}" --define 's3_with_python36_ver8 1')
    yumdep_opts=("${yumdep_opts[@]}" --define 's3_with_python36_ver8 1')
  else
    echo "Building packages for python 3.4 & python 3.6 ..."
    install_python34_deps
    install_python36_deps
    rpmbuild_opts=(--define 's3_with_python34 1' --define 's3_with_python36 1')
    yumdep_opts=(--define 's3_with_python34 1' --define 's3_with_python36 1')
  fi
fi
if [[ build_for_py_34 -eq 1 ]]; then
  echo "Building packages for python 3.4 ..."
  install_python34_deps
  rpmbuild_opts=(--define 's3_with_python34 1')
  yumdep_opts=(--define 's3_with_python34 1')
fi
if [[ build_for_py_36 -eq 1 ]]; then
  echo "Building packages for python 3.6 ..."
  if [[ ($OS = "rhel" || $OS = "centos") && ($VERSION = "\"8.0\"")]]; then
    install_python36_deps_rhel8
  else
    install_python36_deps
  fi
  rpmbuild_opts=("${rpmbuild_opts[@]}" --define 's3_with_python36 1')
  yumdep_opts=("${yumdep_opts[@]}" --define 's3_with_python36 1')
fi
if [[ $pkg = "" ]]; then
  echo "Building all packages $pkg_list..."
else
  echo "Building package $pkg..."
fi

echo "rpmbuild_opts = ${rpmbuild_opts[*]}"
echo "yumdep_opts = ${yumdep_opts[*]}"

WHEEL_VERSION=0.24.0
JMESPATH_VERSION=0.9.0
XMLTODICT_VERSION=0.9.0
S3TRANSFER_VERSION=0.1.10
BOTOCORE_VERSION=1.6.0
BOTO3_VERSION=1.4.6
SCRIPTTEST_VERSION=1.3.0

SCRIPT_PATH=$(readlink -f "$0")
BASEDIR=$(dirname "$SCRIPT_PATH")

cd ~/rpmbuild/SOURCES/
rm -rf wheel*
rm -rf jmespath*
rm -rf xmltodict*
rm -rf s3transfer*
rm -rf botocore*
rm -rf boto3*
rm -rf scripttest*


#wheel
if need_pkg wheel; then
  wget https://bitbucket.org/dholth/wheel/raw/099352e/wheel/test/test-1.0-py2.py3-none-win32.whl
  wget https://bitbucket.org/dholth/wheel/raw/099352e/wheel/test/pydist-schema.json
  wget https://pypi.python.org/packages/source/w/wheel/wheel-${WHEEL_VERSION}.tar.gz
fi

#jmespath
if need_pkg jmespath; then
  wget https://pypi.python.org/packages/source/j/jmespath/jmespath-${JMESPATH_VERSION}.tar.gz
fi

#xmltodict
if need_pkg xmltodict; then
  wget https://pypi.python.org/packages/source/x/xmltodict/xmltodict-${XMLTODICT_VERSION}.tar.gz
fi

#botocore
if need_pkg botocore; then
  wget https://pypi.io/packages/source/b/botocore/botocore-${BOTOCORE_VERSION}.tar.gz
fi

#s3transfer
if need_pkg s3transfer; then
  wget https://pypi.io/packages/source/s/s3transfer/s3transfer-${S3TRANSFER_VERSION}.tar.gz
fi

#boto3
if need_pkg boto3; then
  wget https://pypi.io/packages/source/b/boto3/boto3-${BOTO3_VERSION}.tar.gz
fi

#copy patches
cp ${BASEDIR}/botocore/botocore-1.5.3-fix_dateutil_version.patch ~/rpmbuild/SOURCES/

#scripttest
if need_pkg scripttest; then
  wget https://github.com/pypa/scripttest/archive/${SCRIPTTEST_VERSION}.tar.gz
fi

cd -

# Install python macros
yum info python3-rpm-macros > /dev/null || yum install python3-rpm-macros -y

need_pkg wheel      && (yum-builddep -y ${BASEDIR}/wheel/python-wheel.spec "${yumdep_opts[@]}" ; \
                       rpmbuild -ba ${BASEDIR}/wheel/python-wheel.spec "${rpmbuild_opts[@]}")

need_pkg jmespath   && (yum-builddep -y ${BASEDIR}/jmespath/python-jmespath.spec "${yumdep_opts[@]}" ; \
                       rpmbuild -ba ${BASEDIR}/jmespath/python-jmespath.spec "${rpmbuild_opts[@]}")

need_pkg xmltodict  && (yum-builddep -y ${BASEDIR}/xmltodict/python-xmltodict.spec "${yumdep_opts[@]}" ; \
                        rpmbuild -ba ${BASEDIR}/xmltodict/python-xmltodict.spec "${rpmbuild_opts[@]}")

need_pkg botocore   && (yum-builddep -y ${BASEDIR}/botocore/python-botocore.spec "${yumdep_opts[@]}" ; \
                       rpmbuild -ba ${BASEDIR}/botocore/python-botocore.spec "${rpmbuild_opts[@]}")

need_pkg scripttest && (yum-builddep -y ${BASEDIR}/scripttest/python-scripttest.spec; \
                       rpmbuild -ba ${BASEDIR}/scripttest/python-scripttest.spec)

# These deps are required for s3transfer
yum localinstall ~/rpmbuild/RPMS/noarch/python3* -y

need_pkg s3transfer && (yum-builddep -y ${BASEDIR}/s3transfer/python-s3transfer.spec "${yumdep_opts[@]}" ; \
                       rpmbuild -ba ${BASEDIR}/s3transfer/python-s3transfer.spec "${rpmbuild_opts[@]}")

# s3transfer dep required for boto3
yum localinstall ~/rpmbuild/RPMS/noarch/python3* -y

need_pkg boto3      && (yum-builddep -y ${BASEDIR}/boto3/python-boto3.spec "${yumdep_opts[@]}" ;
                       rpmbuild -ba ${BASEDIR}/boto3/python-boto3.spec "${rpmbuild_opts[@]}")
