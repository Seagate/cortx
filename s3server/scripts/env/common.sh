#!/bin/sh

# sample redhat-release contents for CentOS: CentOS Linux release 7.7.1908 (Core)
# sample redhat-release contents for RedHat: Red Hat Enterprise Linux Server release 7.7 (Maipo)

centos_release=`cat /etc/redhat-release | awk '/CentOS/ {print}'`
redhat_release=`cat /etc/redhat-release | awk '/Red Hat/ {print}'`

os_full_version=""
os_major_version=""
os_minor_version=""
os_build_num=""

if [ ! -z "$centos_release" ]; then
  os_full_version=`cat /etc/redhat-release | awk  '{ print $4 }'`
  os_major_version=`echo $os_full_version | awk -F '.' '{ print $1 }'`
  os_minor_version=`echo $os_full_version | awk -F '.' '{ print $2 }'`
  os_build_num=`echo $os_full_version | awk -F '.' '{ print $3 }'`
elif [ ! -z "$redhat_release" ]; then
  os_full_version=`cat /etc/redhat-release | awk  '{ print $7 }'`
  os_major_version=`echo $os_full_version | awk -F '.' '{ print $1 }'`
  os_minor_version=`echo $os_full_version | awk -F '.' '{ print $2 }'`
fi

# Setup the necessary yum repos as per OS.
if [ "$os_major_version" = "7" ]; then
  if [ "$os_minor_version" = "5" ]; then
    # Centos 7.5
    if [ ! -z "$centos_release" ]; then
      if [ "$os_build_num" = "1804" ]; then
        cp -f ${S3_SRC_DIR}/ansible/files/yum.repos.d/centos7.5.1804/* /etc/yum.repos.d/
      fi
    # RHEL 7.5
    elif [ ! -z "$redhat_release" ]; then
      cp -f ${S3_SRC_DIR}/ansible/files/yum.repos.d/centos7.5.1804/* /etc/yum.repos.d/
    fi

  elif [ "$os_minor_version" = "7" ]; then
    # Centos 7.7
    if [ ! -z "$centos_release" ]; then
      if [ "$os_build_num" = "1908" ]; then
        cp -f ${S3_SRC_DIR}/ansible/files/yum.repos.d/centos7.7.1908/* /etc/yum.repos.d/
      fi
    # RHEL 7.7
    elif [ ! -z "$redhat_release" ]; then
      cp -f ${S3_SRC_DIR}/ansible/files/yum.repos.d/centos7.7.1908/* /etc/yum.repos.d/
    fi
  fi
fi

if [ "$major_version" = "8" ]; then
  # Centos/RHEL 8
  cp -f ${S3_SRC_DIR}/ansible/files/yum.repos.d/rhel8/* /etc/yum.repos.d/
fi

# Install plugin so our repos can take priority
yum install yum-plugin-priorities -y

yum clean all
