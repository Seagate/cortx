=============
CORTX on JBOD
=============
This document provides information on the installation procedures that must be followed to install Cortx on JBOD.

**************
Prerequisites
**************
The prerequisites are as follows:

- Python 3.6

- Root login using password (SSH)

- Salt and gluster_fs

- Image File (ISOs). This file consists of the installation RPMS.

 - The ISO must be placed in a specific location.

***********************
Configuration Workflow
***********************
As a contributor, you must follow the workflow described in the diagram below, to install CORTX on JBOD.

 .. image:: images/JBOD.png
 
***************************************
Setting Motr + hare and IO with m0crate 
***************************************
To setup Motr + hare and IO with m0crate, perform the procedure mentioned below.

1. Install yum utils for yum-config-manager by running the following command.

 - **# yum install yum-utils.noarch**
 
2. Add the latest release rpm repository.

 - **# yum-config-manager --add-repo=http://cortx-storage.colo.seagate.com/releases/eos/github/release-2729/**

3. Add repository for lustre packages.

 - **# sudo yum-config-manager --add-repo=http://cortx-storage.colo.seagate.com/releases/cortx/lustre/custom/tcp/**

4. Add repo for the pacemaker.

 # add /etc/yum.repos.d/base.repo with following contents 
    [base]

 gpgcheck=0

 enabled=1

 baseurl=http://ssc-satellite1.colo.seagate.com/pulp/repos/EOS/Library/custom/CentOS-7/CentOS-7-OS/

 name=base

5. Run the following command to run Motr and Hare.

 - **# yum install -y --nogpgcheck cortx-motr.x86_64 cortx-hare.x86_64**
 
6. Configure lnet on all the nodes. 

   edit /etc/modprobe.d/lnet.conf file with netowork interface used by MOTR endpoints' 
    options lnet networks=o2ib(enp175s0f1) config_on_load=1

 - # systemctl restart lnet

 -  # lctl list_nids

7. To update the BE tx parameters, run the following command:

 -  **# m0provisioning config**
