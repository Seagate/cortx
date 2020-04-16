#!/bin/bash

banner_msg()
{
	echo -e "\n\n"
	echo -e "This scripts prepares a Vanila CentOS host to deploy the Mero multinode cluster with Halon."
	echo -e "This scripts was prepared and tested with the following hardware & software configurations"
	echo -e "\tCPUs: 4"
	echo -e "\tRAM:  12 GB"
	echo -e "\tDisks: HDD1 (10 GB for OS installation); HDD2 (40 GB for Vagrant to use)"
	echo -e "\tKernel Release: 3.10.0-1062.4.1.el7.x86_64"
	echo -e "\tOS: CentOS Linux release 7.7.1908 (Core)"
	echo -e "The host can be a physical machine or a VM (tested with Vmware Workstation on Windows 10) with above provisioning."
	echo -e "The Vagrant version used in this script is [Vagrant 2.2.6]"
	echo -e "This script should be executed by the non-root user, having sudo previleges preferablly with NO_PASSWD, "
	echo -e "This script should ideally succeed, so there is no checking of failure cases or clean-up. Use with CAUTION !!!"
	echo -e "If you are using a VM, take a snapshot before you begin."
	echo -e "\n\n"
	echo -e "\n\n"
}

no_root_and_confirm()
{
	if [ "$UID" == "0" ] || [ "$EUID" == "0" ]; then
		echo -e "\n\n"
		echo -e "This script should not be executed by root, or by sudo. Feel free to try again. No changes were made to your machine."
		echo -e "\n\n"
		exit 1
	fi
	echo -n "Type [YES] to continue. "
	read choice
	if [ "$choice" != "YES" ]; then
		echo -e "\n\n"
		echo -e "Thanks for your time. Feel free to try again. No changes were made to your machine."
		echo -e "\n\n"
		exit 0
	fi
}


## main --- starting from here
banner_msg
no_root_and_confirm


sudo yum -y update
sudo yum -y group install "Development Tools"
sudo yum -y install ansible git qemu-kvm libvirt libvirt-devel libvirt-python libguestfs-tools virt-install virt-manager virt-top bridge-utils
sudo systemctl enable libvirtd
sudo systemctl start libvirtd
wget https://releases.hashicorp.com/vagrant/2.2.6/vagrant_2.2.6_x86_64.rpm
sudo yum -y install ./vagrant_2.2.6_x86_64.rpm
rm -fv ./vagrant_2.2.6_x86_64.rpm
vagrant plugin install vagrant-{env,hostmanager,scp,libvirt}
sudo usermod -a -G libvirt $USER
mv "/usr/bin/kvm" "/usr/bin/kvm--`date +'%d%m%Y_%H%M%S_%s'`"
sudo ln -s /usr/libexec/qemu-kvm /usr/bin/kvm
ls -l /usr/bin/kvm


echo -e "\nThis script will now test the environment by creating a Vanila CentOS 7 box. You can safely press [Ctrl+C] if you know what what you are doing."
echo -en "Press [Y] within 12 sec to continue or any other key to terminate. "
read -t 120 choice
if [ "$choice" != "Y" ]; then
	echo -e "\n\nTERMINATING\n\n"
	exit 0
fi

echo -en "\nIf you already have the box image of the Vanila [CentOS/7] Box, enter its path in 300 sec else press enter to download. "
read -t 300 box_path


cd $HOME
mkdir Centos7
cd Centos7
vagrant init "Centos/7"
if [ "$box_path" == "" ]; then
	vagrant box add Centos/7 --provider=libvirt
else
	vagrant box add "Centos/7" $box_path
fi
vagrant up
vagrant status
vagrant halt


echo -e "\n\n"
echo -e "If there was no errors you host machine is ready to create virtual clusters with m0vg !!!" 
echo -e "Try out the create-cluster.sh."
echo -e "\n\n"

