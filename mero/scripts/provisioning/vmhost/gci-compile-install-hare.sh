#!/usr/bin/env bash
# set -eu -o pipefail ## commands tend to fail unpredictably

cd /data/hare
make
if [ "$?" != 0 ]; then
    echo -e "\n\n\nERROR in Hare compilation!!!\n\n\n"
    echo "TEMP_MSG -- ERROR"
    read a

fi

make rpm
if [ "$?" != 0 ]; then
    echo -e "\n\n\nERROR in creating Hare RPMS!!!\n\n\n"
    echo "TEMP_MSG -- ERROR"
    read a

fi

# sudo cp -rv "/root/rpmbuild/RPMS/x86_64/" /home/vagrant/rpmbuild/RPMS/x86_64/
# if [ "$?" != 0 ]; then
#     echo -e "\n\n\nERROR in copying Hare rpm!!!\n\n\n"
#     echo "TEMP_MSG -- error copying. Do manual copy"
#     read a
# fi

sudo yum -y install /home/vagrant/rpmbuild/RPMS/x86_64/hare-0.*.rpm
if [ "$?" != 0 ]; then
    echo -e "\n\n\nERROR in installing Mero-Devel RPM!!!\n\n\n"
    echo "TEMP_MSG -- error installing. Do manual installation"
    read a
fi

sudo usermod -a -G hare $USER
echo -e "$USER was added to the hare group."
