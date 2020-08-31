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

2. Add repo for the pacemaker.

 # add /etc/yum.repos.d/base.repo with following contents 
    [base]

 gpgcheck=0

 enabled=1

 baseurl=http://ssc-satellite1.colo.seagate.com/pulp/repos/EOS/Library/custom/CentOS-7/CentOS-7-OS/

 name=base

3. Run the following command to run Motr and Hare.

 - **# yum install -y --nogpgcheck cortx-motr.x86_64 cortx-hare.x86_64**
