=============
CORTX on JBOD
=============
This document provides information on the installation procedures that must be followed to install CORTX on JBOD.

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
 
***************************
Configuration of Components
***************************

Configuration of different components that are part of CORTX are mentioned in the sections below.

SSPL
====

The prerequisites and different procedures associated with the configuration of SSPL component are mentioned below.

Prerequisites
-------------

- Provisioner stack must be configured.

 - Provisioner and salt API must be available on setup

- Run the below mentioned command to ensure that RabbitMq server and SSPL rpms are installed.

  ::
  
   $ rpm -qa | grep -E "cortx|rabbitmq" 
   cortx-libsspl_sec-xxxxxxxxxxxxxxxxxxxxx 
   cortx-sspl-xxxxxxxxxxxxxxxxxxxxx 
   cortx-libsspl_sec-method_none-xxxxxxxxxxxxxxxxxxxxx 
   cortx-sspl-test-xxxxxxxxxxxxxxxxxxxxx 
   cortx-prvsnr-cli-xxxxxxxxxxxxxxxxxxxxx 
   cortx-prvsnr-xxxxxxxxxxxxxxxxxxxxx 
   cortx-py-utils-xxxxxxxxxxxxxxxxxxxxx rabbitmq-server-xxxxxxxxxxxxxxxxxxxxx
   
- Run the below mentioned command to ensure that the RabbitMq-server is running and active.

 - **$ systemctl status rabbitmq-server**

- Run the below mentioned command to ensure that the consul agent is running.

 - **$ ps -aux | grep "consul"**
 
Configuration
-------------
Run the below mentioned commands to configure SSPL.

- **$ /opt/seagate/cortx/sspl/bin/sspl_setup post_install -e DEV -p LDR_R1**

- **$ /opt/seagate/cortx/sspl/bin/sspl_setup init -r cortx**

- **$ /opt/seagate/cortx/sspl/bin/sspl_setup config -f**

Note: *-e DEV|PROD <-- is optional. By default, it would be PROD. In order to setup SSPL to start, provisioner feeds sls data into consul (PROD). In JBOD, will provisioner take care of the same or will HA do it?*

Starting and Stopping Services
------------------------------
- Run the following to start the SSPL service.

 - **$ systemctl start sspl-ll**

- Run the following to stop the SSPL service.

 - **$ systemctl stop sspl-ll**

- Run the following to restart the SSPL service.

 - **$ systemctl restart sspl-ll**

- Run the following command to know the status of the SSPL service.

 - **$ systemctl status sspl-ll -l**
 
Verification
------------
Perform sanity test and ensure that the SSPL configuration is accurate. Run the following commands to perform the test.

- **$ /opt/seagate/cortx/sspl/bin/sspl_setup check**

- **$ /opt/seagate/cortx/sspl/bin/sspl_setup test self**

Removing RPM
------------
Reset and uninstall the configuration by running the below mentioned commands.

- **$ /opt/seagate/cortx/sspl/bin/sspl_setup reset hard -p LDR_R1**

- **$ yum remove -y cortx-sspl**

CSM
===

The prerequisites and different procedures associated with the configuration of CSM component are mentioned below.

Prerequisites
-------------
- Consul, ElasticSearch, and RabbitMq must be installed.

- The below mentioned RPMs must be installed on all the nodes.

 - **cortx-csm-agent**

 - **cortx-csm-web**

 - **uds-pyi**
 
Configuration
-------------

Execute the below mentioned commands on the where CSM service would run after fresh installation.

- **csm_setup post_install**

- **csm_setup config**

- **csm_setup init**

You can fine tune the configuration by manually editing the configuration files in **/etc/csm**.

**Note**: In case of UDS, configuration is not required.

Starting Services
-----------------
The starting of services procedure must be performed on only one node.

1. Run the below mentioned commands to start and enable the **csm agent**. 

 - **$ systemctl start csm_agent**

 - **$ systemctl enable csm_agent**

2. Run the below mentioned commands to start and enable the **csm web**.

 - **$ systemctl start csm_web**

 - **$ systemctl enable csm_web**

3. Run the below mentioned commands to start and enable the **UDS**.

 - **$ systemctl start uds**

 - **$ systemctl enable uds**

Ensure that the services have started successfully by running the following command.

- **$ systemctl status <service name>** 

Run the following command to check if the UDS port is open.

- **$ netstat –na | grep 5000**

  Please note that **5000** is the UDS port.

**Note**: After all the services have started running, the CSM web UI is available at port 28100. Navigate to `https://localhost:28100 <https://localhost:28100/>`_ to access the port.

Stopping Services
-----------------

Run the below mentioned commands to stop the CSM service.

- **$ systemctl stop uds**

- **$ systemctl stop csm_web**

- **$ systemctl stop csm_agent**    

HA 
==

The prerequisites and different procedures associated with the configuration of HA component are mentioned below.

Prerequisites
-------------

- Provisioner stack must be configured

 - Provisioner and salt API must be available on setup

- The cortx-ha rpm must be installed

- Installation type identification with provisioner api

 ::

  $ provisioner get_setup_info

  {'nodes': 1, 'servers_per_node': 2, 'storage_type': '5u84', 'server_type': 'virtual'}
  
Configuration
--------------
To check dependency and configure **HA**, perform **post_install**, **config**, and **init**.

- **$ /opt/seagate/cortx/ha/conf/script/ha_setup post_install # call by provisioner (provisioner api)**

- **$ /opt/seagate/cortx/ha/conf/script/ha_setup config**

- **$ /opt/seagate/cortx/ha/conf/script/ha_setup init**

Starting and Stopping Services
------------------------------
In this case, no service is running. Hence, this is not applicable. It is due to the same reason why Verifying (check) is also not applicable.

Command Line Interface (CLI)
----------------------------
- Cluster Management

 - # Start Cortx ha cluster

  - **$ cortxha cluster start**

 - # Stop Cortx-ha cluster

  - **$ cortxha cluster stop**

 - # Get status for services

  - **$ cortxha cluster status**

 - # Shutdown cluster

  - **$ cortxha cluster shutdown**

- Service Management

 The default node value is local.

 - **$ cortx service <service_name> --node <node_id> start**

 - **$ cortx service <service_name> --node <node_id> stop**

 - **$ cortx service <service_name> --node <node_id> status**

 **Note**: The name (Services Name) in the above CLI is **Hare**.
 
Removing RPM
------------
Reset and uninstall the configuration by running the below mentioned commands.

- **$ /opt/seagate/cortx/ha/conf/script/ha_setup reset**

- **$ yum remove cortx-ha**

