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
Perform the the first 4 steps on the 3 nodes with the following change in **olcseverid.ldif**.

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
  
3. Push the provider ldif for config replication.

   **syncprov_config.ldif**

 ::

  dn: olcOverlay=syncprov,olcDatabase={0}config,cn=config

  objectClass: olcOverlayConfig

  objectClass: olcSyncProvConfig 

  olcOverlay: syncprov

  olcSpSessionLog: 100 


 **command to add - ldapadd -Y EXTERNAL -H ldapi:/// -f  syncprov_config.ldif**
 
4. Push the **Config.ldif** file.

     **config.ldif**

        ::

          dn: olcDatabase={0}config,cn=config 

          changetype: modify 

          add: olcSyncRepl 

          olcSyncRepl: rid=001

              provider=ldap://<hostname_node-1>:389/ 

              bindmethod=simple 

              binddn="cn=admin,cn=config" 

              credentials=seagate 

              searchbase="cn=config" 

              scope=sub 

              schemachecking=on 

              type=refreshAndPersist 

              retry="30 5 300 3" 

              interval=00:00:05:00

         # Enable additional providers 

         olcSyncRepl: rid=002 

            provider=ldap://<hostname_node-2>:389/ 

            bindmethod=simple 

            binddn="cn=admin,cn=config" 

            credentials=seagate 

            searchbase="cn=config" 

            scope=sub 

            schemachecking=on 

            type=refreshAndPersist 

            retry="30 5 300 3" 

            interval=00:00:05:00 

         olcSyncRepl: rid=003 

            provider=ldap://<hostname_node-3>:389/ 

            bindmethod=simple 

            binddn="cn=admin,cn=config" 

            credentials=seagate 

            searchbase="cn=config" 

            scope=sub 

            schemachecking=on 

            type=refreshAndPersist 

            retry="30 5 300 3" 

            interval=00:00:05:00 

         add: olcMirrorMode 

         olcMirrorMode: TRUE
        

        **command to add - ldapmodify -Y EXTERNAL  -H ldapi:/// -f config.ldif**
        
Perform the following steps on only one node. In this case, it must be performed on the primary node.

1. Push  the provider for data replication.

   ::

    syncprov.ldif

     dn: olcOverlay=syncprov,olcDatabase={2}mdb,cn=config 

     objectClass: olcOverlayConfig 

     objectClass: olcSyncProvConfig 

     olcOverlay: syncprov 

     olcSpSessionLog: 100


   **command to add - ldapadd -Y EXTERNAL -H ldapi:/// -f  syncprov.ldif**
   
2. Push the data replication ldif.

  **data.ldif**

  ::

    dn: olcDatabase={2}mdb,cn=config 

    changetype: modify 

    add: olcSyncRepl 

    olcSyncRepl: rid=004

       provider=ldap://< hostname_of_node_1>:389/ 

       bindmethod=simple 

       binddn="cn=admin,dc=seagate,dc=com" 

       credentials=seagate 

       searchbase="dc=seagate,dc=com" 

       scope=sub 

       schemachecking=on 

       type=refreshAndPersist 

       retry="30 5 300 3" 

       interval=00:00:05:00

     # Enable additional providers

     olcSyncRepl: rid=005

        provider=ldap://< hostname_of_node_2>:389/ 

        bindmethod=simple 

        binddn="cn=admin,dc=seagate,dc=com" 

        credentials=seagate 

        searchbase="dc=seagate,dc=com" 

        scope=sub 

        schemachecking=on 

        type=refreshAndPersist 

        retry="30 5 300 3" 

        interval=00:00:05:00 

      olcSyncRepl: rid=006   

         provider=ldap://<hostname_of_node_3>:389/ 

         bindmethod=simple 

         binddn="cn=admin,dc=seagate,dc=com" 

         credentials=seagate 

         searchbase="dc=seagate,dc=com" 

         scope=sub 

         schemachecking=on 

         type=refreshAndPersist 

         retry="30 5 300 3" 

         interval=00:00:05:00

   

       add: olcMirrorMode 

       olcMirrorMode: TRUE
  

**command to add - ldapmodify -Y EXTERNAL -H ldapi:/// -f data.ldif**

   **Note**: Update the host name in the provider field in data.ldif before running the command.

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

S3 (AuthServer and HAProxy)
===========================

AuthServer
----------

The AuthServer is configured along with the installation of S3 component.

Starting and Stopping Services
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- Run the below mentioned command to start the AuthServer.

 - **systemctl start s3authserver**

- Run the below mentioned command to restart the AuthServer.

 - **systemctl restart s3authserver**
 
- Run the below mentioned command to stop the AuthServer.

 - **systemctl stop s3authserver**
 
- Run the following command to check the status of AuthServer.

 - systemctl status s3authserver

HAProxy
--------
This section provides information on the installation and configuration associated with HAProxy.

Installation
^^^^^^^^^^^^^
1. Run the following command to install HAProxy.

 - **yum install haproxy**

2. Copy the **s3server.pem** file from `here <https://github.com/Seagate/cortx-s3server/tree/dev/ansible/files/certs/stx-s3/s3>`_ and paste it in the **/etc/ssl/stx-s3/s3/** directory.

3. Navigate to **/opt/seagate/cortx/s3/install/haproxy**.

4. Copy the contents of **haproxy_osver7.cfg** (**haproxy_osver8.cfg** depending on your OS version) to **/etc/haproxy/haproxy.cfg**.

Configuration
^^^^^^^^^^^^^^
Before configuring HAProxy, check the number of S3 instances using **hctl status**. The hctl status would be similar to the below content.

::

 Profile: 0x7000000000000001:0xc0Data pools:
 0x6f00000000000001:0xc1Services:    sm18-r20.pun.seagate.com    [started]
 hax        0x7200000000000001:0x84  192.168.20.18@o2ib:12345:1:1    [started]  
 confd      0x7200000000000001:0x87  192.168.20.18@o2ib:12345:2:1    [started]  
 ioservice  0x7200000000000001:0x8a  192.168.20.18@o2ib:12345:2:2    [started]  
 s3server   0x7200000000000001:0xae  192.168.20.18@o2ib:12345:3:1    [started]  
 s3server   0x7200000000000001:0xb1  192.168.20.18@o2ib:12345:3:2    [started]  
 s3server   0x7200000000000001:0xb4  192.168.20.18@o2ib:12345:3:3    [started]  
 s3server   0x7200000000000001:0xb7  192.168.20.18@o2ib:12345:3:4    [unknown]  
 m0_client  0x7200000000000001:0xba  192.168.20.18@o2ib:12345:4:1    [unknown]  
 m0_client  0x7200000000000001:0xbd  192.168.20.18@o2ib:12345:4:2    sm10-
 r20.pun.seagate.com    [started]  hax        0x7200000000000001:0x6   
 192.168.20.10@o2ib:12345:1:1    [started]  confd      0x7200000000000001:0x9   
 192.168.20.10@o2ib:12345:2:1    [started]  ioservice  0x7200000000000001:0xc   
 192.168.20.10@o2ib:12345:2:2    [started]  s3server   0x7200000000000001:0x30  
 192.168.20.10@o2ib:12345:3:1    [started]  s3server   0x7200000000000001:0x33  
 192.168.20.10@o2ib:12345:3:2    [started]  s3server   0x7200000000000001:0x36  
 192.168.20.10@o2ib:12345:3:3    [started]  s3server   0x7200000000000001:0x39  
 192.168.20.10@o2ib:12345:3:4    [unknown]  m0_client  0x7200000000000001:0x3c  
 192.168.20.10@o2ib:12345:4:1    [unknown]  m0_client  0x7200000000000001:0x3f  
 192.168.20.10@o2ib:12345:4:2    sm11-r20.pun.seagate.com  (RC)    [started]  
 hax        0x7200000000000001:0x45  192.168.20.11@o2ib:12345:1:1    [started]  
 confd      0x7200000000000001:0x48  192.168.20.11@o2ib:12345:2:1    [started]  
 ioservice  0x7200000000000001:0x4b  192.168.20.11@o2ib:12345:2:2    [started]  
 s3server   0x7200000000000001:0x6f  192.168.20.11@o2ib:12345:3:1    [started]  
 s3server   0x7200000000000001:0x72  192.168.20.11@o2ib:12345:3:2    [started]  
 s3server   0x7200000000000001:0x75  192.168.20.11@o2ib:12345:3:3    [started]  
 s3server   0x7200000000000001:0x78  192.168.20.11@o2ib:12345:3:4    [unknown]  
 m0_client  0x7200000000000001:0x7b  192.168.20.11@o2ib:12345:4:1    [unknown]  
 m0_client  0x7200000000000001:0x7e  192.168.20.11@o2ib:12345:4:2
 
From the above result, it can be seen that each node has 4 s3server instances. Hence, each HAProxy will be configured with 4 (s3 instances) x 3 (nodes) = 12 S3 instances in the HAProxy’s  **backend** section of app-main. Let us consider this value of number of S3 instances per node as **N**. Perform the steps mentioned below to configure **N**.

1. Open **/etc/haproxy/haproxy.cfg** from the active node, and navigate to the **backend app-main** section.

2. Locate the S3 instance - **server s3-instance-1 0.0.0.0:28081 check maxconn 110**. Add **N – 1**. In case of VM, if the number of S3 instances per node is 1, then three steps (2,3,4) including this will be skipped.

3. Name instances uniquely **(s3-instance-x)** and increment **x** by 1, for every instance.

4. Increment the port number (**28081**) for the next 3 instances, by 1. 

5. Navigate to **backend s3-auth** section, and comment out the **HAProxy Monitoring Config** section if present.

6. Copy the **haproxy.cfg** to the other server nodes at the same location - **/etc/haproxy/haproxy.cfg**. 

7. Configure haproxy logs on all the nodes by running the following commands.

 - **mkdir /etc/haproxy/errors/** 

 - **cp /opt/seagate/cortx/s3/install/haproxy/503.http /etc/haproxy/errors/**

 - **cp /opt/seagate/cortx/s3/install/haproxy/logrotate/haproxy /etc/logrotate.d/haproxy** 

 - **cp /opt/seagate/cortx/s3/install/haproxy/rsyslog.d/haproxy.conf /etc/rsyslog.d/haproxy.conf** 

 - **rm -rf /etc/cron.daily/logrotate** 

 - **cp /opt/seagate/cortx/s3/install/haproxy/logrotate/logrotate /etc/cron.hourly/logrotate** 

 - **systemctl restart rsyslog** 

 - **systemctl restart haproxy** 

 - **systemctl status haproxy**
 
Starting and Stopping Services
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
 
- Run the below mentioned command to start the HAProxy services.

 - **systemctl start haproxy**
 
- Run the below mentioned command to stop the HAProxy services.

 - **systemctl stop haproxy**
 
- Run the below mentioned command to check the status of HAProxy services.

 - **systemctl status haproxy**

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

- **$ /opt/seagate/cortx/sspl/bin/sspl_setup post_install -p LDR_R1**

- **$ /opt/seagate/cortx/sspl/bin/sspl_setup init -r cortx**

- **$ /opt/seagate/cortx/sspl/bin/sspl_setup config -f**


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


Starting Services
-----------------
The starting of services procedure must be performed on only one node.

1. Run the below mentioned commands to start and enable the **csm agent**. 

 - **$ systemctl start csm_agent**

 - **$ systemctl enable csm_agent**

2. Run the below mentioned commands to start and enable the **csm web**.

 - **$ systemctl start csm_web**

 - **$ systemctl enable csm_web**

Ensure that the services have started successfully by running the following command.

- **$ systemctl status <service name>** 


**Note**: After all the services have started running, the CSM web UI is available at port 28100. Navigate to **https://<IP address of the box>:28100** to access the port.

Stopping Services
-----------------

Run the below mentioned commands to stop the CSM service.


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

