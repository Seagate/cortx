===================================
Running Distributed Scale-out CORTX 
===================================
This document provides information on the installation procedures that must be followed to install a set of CORTX servers doing distributed erasure across a set of storage devices.

**************
Limitations
**************
Please note that this is a preview of distributed CORTX doing network erasure and it is not yet failure resilient.  Please do not use these instructions to run CORTX for production reasons nor for storing any critical data.  Do note that the officially supported Seagate version of Lyve Drive Rack can be used for production reasons since it relies on erasure within the enclosures.

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
 
*****************************
Configuration of Dependencies
*****************************

The procedures that must be followed to install and configure different dependencies are mentioned below.

LDAP
====
This section describes the procedures that must be followed to configure LDAP.

Prerequisites
--------------
- S3 Server must be installed.

- 3 VMs must be available

Configuration
-------------

1. Navigate to **/opt/seagate/cortx/s3/install/ldap**.

2. Run **setup_ldap.sh** using the following command.

 - **./setup_ldap.sh --defaultpasswd --skipssl --forceclean**

3. After LDAP is setup on the three nodes, perform **LDAP Replication**. Refer the procedure below.

4. Configure **slapd.log** on all 3 nodes using the commands mentioned below.

 - **cp /opt/seagate/cortx/s3/install/ldap/rsyslog.d/slapdlog.conf /etc/rsyslog.d/slapdlog.conf** 
 
 - **systemctl restart slapd**

 - **systemctl restart rsyslog**

Starting and Stopping Services
------------------------------

- Run the following command to start the service.

 - **systemctl start slapd**

- Run the following command to stop the service.

 - **systemctl stop slapd**

Run the following command to check the status of the service.

- **systemctl status slapd**

LDAP Replication
----------------
This section consists of the prerequisites and procedure associated with the ldap replication.

Prerequisites
^^^^^^^^^^^^^
- LDAP must be installed.

- 3 nodes must be available

- The host name in the provider field in **config.ldif** on all 3 nodes if not updated earlier.

Procedure
^^^^^^^^^^
Perform the the initial 4 steps with the following change in **olcseverid.ldif**.

- **olcseverrid  = 1 for node 1**

- **olcseverrid  = 2 for node 2**

- **olcseverrid  = 3 for node 3**

1. Push the unique olcserver Id.
   
   **olcserverid.ldif**

  ::

   dn: cn=config
   changetype: modify
   add: olcServerID
   olcServerID: 1

 **command to add -: ldapmodify -Y EXTERNAL -H ldapi:/// -f olcserverid.ldif**

2. Load the provider module.

   **syncprov_mod.ldif**

   ::

    dn: cn=module,cn=config
    objectClass: olcModuleList
    cn: module
    olcModulePath: /usr/lib64/openldap
    olcModuleLoad: syncprov.la

  **command to add - ldapadd -Y EXTERNAL -H ldapi:/// -f syncprov_mod.ldif**

RabbitMQ
========
This section describes the procedures that must be followed to configure RabbitMQ.

Prerequisites
--------------
- Provisioner stack must be configured.

 - Provisioner and salt API must be available on setup

- The RabbitMQ - server rpm must be installed in the system.

 - $rpm -qa | grep "rabbitmq"

   rabbitmq-server-3.3.5-34.el7.noarch

- Data from the **rabbitmq.sls** file must be transmitted into consul. This action is performed by provisioner.

- Run the below mentioned script to avoid RMQ processor related errors.

 - **$ python3 /opt/seagate/cortx/provisioner/cli/pillar_encrypt** 

- The **erlang.cookie** file must be available. Run the following command to check the availability.

 - **$ cat /var/lib/rabbitmq/.erlang.cookie**
 
Configuration
-------------
1. Start the RabbitMQ server.
2. Open the required ports for rabbitmq.

 ::

  systemctl start firewalld 
  firewall-cmd --zone=public --permanent --add-port=4369/tcp 
  firewall-cmd --zone=public --permanent --add-port=25672/tcp 
  firewall-cmd --zone=public --permanent --add-port=25672/tcp 
  firewall-cmd --zone=public --permanent --add-port=5671-5672/tcp 
  firewall-cmd --zone=public --permanent --add-port=15672/tcp 
  firewall-cmd --zone=public --permanent --add-port=15672/tcp 
  firewall-cmd --zone=public --permanent --add-port=61613-61614/tcp 
  firewall-cmd --zone=public --permanent --add-port=1883/tcp 
  firewall-cmd --zone=public --permanent --add-port=8883/tcp 
  firewall-cmd --reload

Starting and Stopping
---------------------
- Run the below mentioned command to start the server.

 - **$ systemctl start rabbitmq-server**

- Run the below mentioned command to stop the server.

 - **$ systemctl stop rabbitmq-server**

- Run the below mentioned command to restart the server.

 - **$ systemctl restart rabbitmq-server**

Run the below mentioned command to know the status.

 - **$ systemctl status rabbitmq-server -l**

Statsd and Kibana
=================
This section describes the procedures that must be followed to configure statsd and kibana.

- **Statsd** is used to collect metric from various sources and it runs on each node as the daemon service.

- **Kibana** is used to aggregate metrics and run on the system with csm service.

Prerequisites
-------------

- The following RPMs must be available.

 - **statsd**

 - **stats_utils**

 - **kibana**

Statsd Configuration
--------------------
Run the below mentioned commands to start and enable the **statsd** service. This must be performed on every node.

- **$ systemctl start statsd**

- **$ systemctl enable statsd**

To know the status of the service, run the following command.

- **$ systemctl status statsd**

Kibana Configuration
--------------------
1. Update the **kibana.service** file on each system. By default, the service is not compatible with new systemd. Run the following command to check the compatibility.

 - **$ systemd-analyze verify /etc/systemd/system/kibana.service**

  - If above command gives a warning, replace the file with **/etc/systemd/system/kibana.service**.

  In the orignal kibana.service file, **StartLimitInterval** and **StartLimitBurst** are part of **Unit** Section but as per new systemd rule it is part of **Service** section.

 ::

  Description=Kibana
 
  [Service] 
  Type=simple 
  StartLimitInterval=30 
  StartLimitBurst=3 
  User=kibana 
  Group=kibana 
  # Load env vars from /etc/default/ and /etc/sysconfig/ if they exist. 
  # Prefixing the path with '-' makes it try to load, but if the file doesn't 
  # exist, it continues onward. 
  EnvironmentFile=-/etc/default/kibana 
  EnvironmentFile=-/etc/sysconfig/kibana 
  ExecStart=/usr/share/kibana/bin/kibana "-c /etc/kibana/kibana.yml" 
  Restart=always 
  WorkingDirectory=/ 

  [Install] WantedBy=multi-user.target
  
2. Reload the daemon on each system by running the following command.

 - **$ systemctl daemon-reload**

3. Find the active csm service (active node) by running the following command.

 - **$ systemctl status csm_agent**

4. Start kibana on the active CSM node and enable the service by running the following commands.

 - **$ systemctl start kibana**

 - **$ systemctl enable kibana**

Check the systemd status on active CSM node by running the following command.

 - **$ systemctl status kibana**
 
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

Run the following command to know the status of the SSPL service.

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

