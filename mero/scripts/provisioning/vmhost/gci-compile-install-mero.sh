#!/usr/bin/env bash
# set -eu -o pipefail ## commands tend to fail unpredictably

cd /data/mero
# ./scripts/m0 rebuild
./autogen.sh
if [ "$?" != 0 ]; then
    echo -e "\n\n\nERROR in Mero compilation; autogen.sh failed!!!\n\n\n"
    exit -1
fi

./configure
if [ "$?" != 0 ]; then
    echo -e "\n\n\nERROR in Mero compilation; ./configure failed!!!\n\n\n"
    exit -1
fi

make -j4
if [ "$?" != 0 ]; then
    echo -e "\n\n\nERROR in Mero compilation; [make] failed!!!\n\n\n"
    exit -1
fi

make rpms-notests
if [ "$?" != 0 ]; then
    echo -e "\n\n\nERROR in creating Mero RPMS!!!\n\n\n"
    exit -1
fi

sudo yum -y install /home/vagrant/rpmbuild/RPMS/x86_64/eos-core-1.0.*.rpm
if [ "$?" != 0 ]; then
    echo -e "\n\n\nERROR in installing Mero RPM!!!\n\n\n"
    read a
fi

sudo yum -y install /home/vagrant/rpmbuild/RPMS/x86_64/eos-core-devel-1.0.*.rpm
if [ "$?" != 0 ]; then
    echo -e "\n\n\nERROR in installing Mero-Devel RPM!!!\n\n\n"
    read a
fi

