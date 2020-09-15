###################################
Running Distributed Scale-out CORTX 
###################################
This document details the installation procedures that must be followed to install a set of CORTX servers doing distributed erasure across a set of storage devices.

**Important**: Please note that this is a preview of distributed CORTX doing network erasure and it is not failure resilient. Do not use these instructions to run CORTX for production reasons nor for storing critical data. The official Seagate version of Lyve Drive Rack (LDR) can be used for production reasons as it relies on erasure within the enclosures.

*********************************
3 Node JBOD Setup (Prerequisites)
*********************************

Perform the below mentioned procedure to complete the process of 3 node JBOD Setup.

1. Prepare three servers and three JBODs as per the following guidelines.

 a. Server Reference Configuration

  - Minimal Configuration

   - 1x Intel Xeon CPU, 6 cores per CPU (2x Intel Xeon CPU, 10 cores per CPU for optimal performance)

   - 64 GB RAM ( 192 GB RAM for optimal performane)

   - 2x 1 TB internal HDD

   - One dual-port or two single-port Mellanox HCA (for the data networks)

   - At least one 1 GbE network port (for the Management network)

   - SCSI HBA with expernal ports (to connect to JBOD)

   **Notes**

   - The minimum number of network ports per server is 3.

   - Usage of Mellanox HCAs is recommended but not mandatory. For optimal performance you need two high-speed network ports (10 GbE minimum; 50 GbE or 100 GbE recommended).

    - All the three servers must have Mellanox HCA or none of the servers must have it.

 b. JBOD Reference Configuration

  - The minimum number of disks per JBOD is 7. One JBOD must be connected to one server. The minimum size of the JBOD disk is 2TB.

 c. Network Configuration Requirements

  - The CORTX software requires 3 separate networks. The networks could be physically separate (connected to different switches) or separate VLANs. We recommend you to physically separate the management and data networks.

         +--------------------------+---------------------------------------------+
         | **Network name/purpose** | **Corresponding NIC**                       |
         +--------------------------+---------------------------------------------+
         | Management network       | connected to the 1 GbE NIC                  |
         +--------------------------+---------------------------------------------+
         | Public Data network      | connected to the one of the high-speed NICs |
         +--------------------------+---------------------------------------------+
         | Private Data network     | connected to another high-speed NIC         |
         +--------------------------+---------------------------------------------+

2. Connect the servers to the networks and the JBODs as per the guidelines provided above.

3. Install CentOS 7.7 (1908 release) operating system on all three servers in the future cluster.

  **Note**: The release must match exactly, as the other versions and distributions of Linux are not supported. You can verify the release by running the following commands and view the appropriate outputs.
  
 - **lsb_release -r**

   - Appropriate Output: 7.7.1908

 - **uname -r**

  - Appropriate Output: 3.10.0-1062.el7.x86_64
  
  **Warning**: Do not update CentOS 7.7 release as it will break CORTX. Operating system updates are not supported at the moment.

  While there are no specific requirements for installing the CentOS 7.7, we recommend you to perform the following 4 steps.

  a. Use at least two identical internal HDDs in each server (see Server Reference Configuration above).

  b. On each drive, configure the partitions as per the following guidelines.

     +-----------------------+-------------+-------------------------------------------+
     | **Partition number**  |  **Size**   |        **Notes**                          |
     |                       |             |                                           |
     +-----------------------+-------------+-------------------------------------------+
     |     1                 | 256 MB      | to be mounted to /boot/efi or /boot/efi2  |
     +-----------------------+-------------+-------------------------------------------+
     |     2                 |  1 GB       | to be used as part of md0 RAID-1 volume   |
     +-----------------------+-------------+-------------------------------------------+
     |     3                 | rest of     | to be used as part of md1 RAID-1 volume   |
     |                       | disk        |                                           |
     +-----------------------+-------------+-------------------------------------------+

    **Note**: The partitioning schema is assuming the servers support UEFI for booting. If the servers do not support UEFI, partition #1 is not required. CentOD Linux implementation of UEFI does not support RAID configuration at the moment, therefore two separate EFI partitions will be needed to be able to boot the server in case of one of the disk fails. These partions should be mounted to /boot/efi (the partition on disk #1) and /boot/efi2 (the partition on disk #2).
    
   c. Create two RAID-1 volumes.

   +------------------+------------------------------------------+
   | **Volume name**  |   **Purpose / mount point**              |
   |                  |                                          |
   +------------------+------------------------------------------+
   |  md0             |  /boot                                   |
   +------------------+------------------------------------------+
   |  md1             |  To be used as physical volume for LVM   |
   +------------------+------------------------------------------+

   d. Create LVM configuration for the remaining OS partitions using md1 RAID-1 volume. We recommend you the following LVM disk group and volumes structure.

    +--------------------------------+-----------------+----------+--------------+
    |    **LVM device name**         | **Mount point** | **Size** | **FS type**  |
    |                                |                 |          |              |
    +--------------------------------+-----------------+----------+--------------+
    | /dev/mapper/vg_sysvol-lv_root  | /               | 200GB    | ext4         |
    +--------------------------------+-----------------+----------+--------------+
    | /dev/mapper/vg_sysvol-lv_tmp   | /tmp            | 200GB    | ext4         |
    +--------------------------------+-----------------+----------+--------------+
    | /dev/mapper/vg_sysvol-lv_var   | /var            | 200GB    | ext4         |
    +--------------------------------+-----------------+----------+--------------+
    | /dev/mapper/vg_sysvol-lv_log   | /var/log        | 200GB    | ext4         |
    +--------------------------------+-----------------+----------+--------------+
    | /dev/mapper/vg_sysvol-lv_audit | /var/log/audit  | 128MB    | ext4         |
    +--------------------------------+-----------------+----------+--------------+
    | /dev/mapper/vg_swap            | none            | 100GB    | linux-swap(*)|
    +--------------------------------+-----------------+----------+--------------+

    **Note**: The information in the table above is provided for reference purposes. You can choose a different structure and/or use different sizes for the partitions (LVM volumes). The minimal size of the / (root) partition should be 20 GB to allow installation of the operating system and the CORTX software. Please adjust the size or / (root) partition accordingly if you do not create separate /var and /var/log partitions.
    
4. Configure root user on all 3 servers to use the same password. This is required for the installation and can be changed after the installation is complete.

5. Allow the root login over SSH on all three servers. This is required for the installation and operations of the cluster.

   **Notes**

    - This setting cannot be changed after the installation is complete.

    - You can create another non-root user to avoid logging in to the servers as root all the time. Please allow this user to run all commands using sudo (add it to the "wheel" group).
    
6. If you have Mellanox HCAs on your servers, please proceed to the next step. If not, proceed to step 8.

7. Install Mellanox OFED from http://linux.mellanox.com/public/repo/mlnx_ofed/4.7-3.2.9.0/rhel7.7/x86_64/MLNX_LIBS/. You must reboot the system after completing the installation.

  - Supported Version - 4.7-3.2.9.0

   - Other versions are not supported.

8. Download CORTX ISO and CORTX 3rd_party ISO files from <url to github location>.

9. Upload the ISOs to the first server in the cluster that you are planning to install. It is recommended to have the ISOs in the same location.

10. On all three servers, setup Python 3.6 virtual environment. Refer https://docs.python.org/3.6/library/venv.html.

   - Supported Version - 3.6
   
    - Other versions are not supported.
    
11. Configure DNS and DHCP server, if used, with the host names and IP addresses for each server.

  - Each server should have FQDN assigned to it. The FQDN should be associated with the IP address of the management network interface.

  - Configure IP addresses on Management and Public Data network interfaces on each server using one of the following methods:

   - static IP addresses for each of the network interfaces

   - dynamic IP addresses for each of the network interfaces

   **Important Notes**

   - CORTX does not support IPv6. Only IPv4 is supported.

   - If you are using dynamic IP addresses, please map the MAC addresses of the respective interfaces to the IP address in the configuration of your DHCP server. This is required to avoid possible IP changes when the leases associated with DHCP expire.

   - If DHCP server is used, ensure that DHCP server passes host names to the servers.

   - Do not configure DHCP to assign the IP address to the private data interfaces. This interface is configured by the CORTX software installer. By default, the configuration uses **192.168.0.0/24** subnet. This setting can be changed by providing necessary information in the config.ini file. For more information, move to step 12.

   You also need two static IPs to be used as Virtual IPs (VIPs). One VIP will be used as Management VIP and another VIP will be used as Cluster (Data) VIP.

   - The Management VIP should be from the same subnet as the rest of the Management network IPs.

   - The Cluster (Data) VIP should be from the same subnet as the rest of the Public Data network IPs.

   **Notes**
 
   - VIPs utilize CLUSTERIP iptables module that relies on multicast. For CORTX to function appropriately, multicasts should be allowed for Management and Public Data networks.


   - These static IPs are required regardless of whether DHCP is used to provide IP addresses for each server interface or not.

   - You must configure DNS resolution for these VIPs.
   
12. Collect all the required information and prepare **config.ini** file for your installation. Refer to `Config.ini File <https://github.com/Seagate/cortx/blob/main/doc/Description%20of%20config.ini.rst>`_ for complete information. After the file is prepared, upload it to the first server in the cluster you are planning to install.

******************************
Installation of CORTX Software
******************************

This section provides information on the installation of Provisioner and the associated API. Perform the below mentioned procedure to complete the process of installation.

1. Login to the first server of the cluster that you want to install and become root.

2. Start the **screen** or **tmux** session to avoid the stalling of installation. If these utilities are not present, install them using CentOS tools (yum).

3. Run the below mentioned command to install the CORTX Provisioner API.

  ::

    pip install https://github.com/Seagate/provisioner-test/releases/download/cortx-api-v0.33.0/cortx-prvsnr-0.33.0.tar.gz

4. Run the below mentioned commands to install the cluster. The approximate time taken is 40 minutes.

  ::

   provisioner setup_jbod --source iso --iso-cortx <path_to_CORTX_ISO> \
    --iso-cortx-deps <path_to_3rd_party_ISO> \
    --ha --logfile --logfile-filename <path_to_logfile> \
    --config-path <path_to_config.ini> srvnode-1:<server-1_fqdn> \
    srvnode-2:<server-2_fqdn> srvnode-3:<server-3_fqdn>

  where

  ::

    --source            Installation source (only ISO files are supported at the moment)
    --iso-cortx         Path to CORTX ISO location
    --iso-cortx-deps    Path to ISO with 3rd party software
    --ha                Enable high-availability
    --logfile           Create a log file for the installation
    --logfile-filename  Path to and the name of the log file where the installation log will be written
    --config-path       Path to config.ini file
    srvnode-1:<host>    FQDN of server-1
    srvnode-2:<host>    FQDN of server-2
    srvnode-3:<host>    FQDN of server-3

  For example:

  ::

   provisioner setup_jbod --source iso --iso-cortx /root/cortx.iso \
    --iso-cortx-deps /root/prereqs.iso --ha --logfile \
    --logfile-filename ./setup.log --config-path config.ini \
    srvnode-1:srv1.test.com srvnode-2:srv2.test.com srvnode-3:srv3.test.com

5. Run the below mentioned commands to verify that the dependency components are installed successfully.

   :: 
 
    /usr/share/kibana/bin/kibana --version
    
    slapd -V

    /usr/share/elasticsearch/bin/elasticsearch --version**

    rabbitmqadmin --version

    node --version

    lfs --version

 The output must be displayed in the following tabular format.

+---------------+-----------------------------------------------------+
| **Component** |                 **Expected output**                 |
+---------------+-----------------------------------------------------+
| kibana        | 6.8.8                                               |
+---------------+-----------------------------------------------------+
| OpenLDAP      | @(#) $OpenLDAP: slapd 2.4.44 (Jan 29 2019 17:42:45) |
|               | $mockbuild@x86-01.bsys.centos.org:/builddir/build/  |
|               | BUILD/openldap-2.4.44/openldap-2.4.44/servers/slapd |
+---------------+-----------------------------------------------------+
| ElasticSearch | Version: 6.8.8, Build: oss/                         |
|               | rpm/2f4c224/2020-03-18T23:22:18.622755Z,            |
|               | JVM: 1.8.0_242                                      |
+---------------+-----------------------------------------------------+
| RabbitMQ      | rabbitmqadmin 3.3.5                                 |
+---------------+-----------------------------------------------------+
| NodeJS        | v6.17.1                                             |
+---------------+-----------------------------------------------------+
| LFS           | lfs 2.12.3                                          |
+---------------+-----------------------------------------------------+

6. Proceed to the next section, and start the configuration procedures.
 
***************************************
I/O Configuration (Motr + HARE + S3) 
***************************************
Perform the below mentioned procedure to configure the I/O stack.

1. Update the BE tx parameters by running the below mentioned command. The **/etc/sysconfig/motr** gets configured.

   ::
    
    m0provision config

2. Run the below mentioned command to bootstrap the cluster.

   ::

    hctl bootstrap --mkfs cluster.yaml

  This command must be used with **mkfs** only while running it for the first time. 

3. Verify the motr utility m0crate, by creating a sample m0crate workload file and running m0crate workload. Run the below mentioned commands.

   ::

    /opt/seagate/cortx/hare/libexec/m0crate-io-conf > /tmp/m0crate-io.yaml

    m0crate -S /tmp/m0crate-io.yaml

Run the below mentioned command to start the cluster. This command must be used while starting the cluster from second time.

 ::

  hctl bootstrap –c /var/lib/hare 
 
*****************************
Configuration of Dependencies
*****************************

The procedures that must be followed to install and configure different dependencies are mentioned below.

LDAP
====
This section describes the procedures that must be followed to configure LDAP.

Configuration
-------------

1. Navigate to **/opt/seagate/cortx/s3/install/ldap**. This is applicable to all the 3 nodes.

2. Run **setup_ldap.sh** using the following command.

   ::

     ./setup_ldap.sh --defaultpasswd --skipssl --forceclean

3. After LDAP is setup on the three nodes, perform **LDAP Replication**. Refer the procedure below.

4. Configure **slapd.log** on all 3 nodes using the commands mentioned below.

   ::

     cp /opt/seagate/cortx/s3/install/ldap/rsyslog.d/slapdlog.conf /etc/rsyslog.d/slapdlog.conf
     systemctl restart slapd
     systemctl restart rsyslog

Starting Service
-----------------

- Run the following command to start the service on all the 3 nodes.

  ::

    systemctl start slapd

Run the following command to check the status of the service.

::

  systemctl status slapd

LDAP Replication
----------------
This section consists of the prerequisites and procedure associated with the ldap replication. The procedure below must be performed on all the 3 nodes.

Prerequisite
^^^^^^^^^^^^

- The host name must be updated in the provider field in **config.ldif** on all the 3 nodes.

**Note**: All the commands must run successfully. The below mentioned errors must not occur.

- *no such attribute*

- *invalid syntax*

**Important**
^^^^^^^^^^^^^

You need not copy the contents of the files from this page as they are placed in the following directory.

::

  cd /opt/seagate/cortx/s3/install/ldap/replication
 
Edit the relevant fields as required (olcserverid.ldif and config.ldif). 

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

- Run the below mentioned script to avoid RMQ processor related errors.

   ::

    python3 /opt/seagate/cortx/provisioner/cli/pillar_encrypt 

- The **erlang.cookie** file must be available. Run the following command to check the availability.

   ::

    cat /var/lib/rabbitmq/.erlang.cookie
 
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

Starting Service
-----------------
- Run the below mentioned command to start the server.

   ::

    systemctl start rabbitmq-server

- Run the below mentioned command to restart the server.

   ::

    systemctl restart rabbitmq-server

Run the below mentioned command to know the status.

 ::

  systemctl status rabbitmq-server -l

Statsd and Kibana
=================
This section describes the procedures that must be followed to configure statsd and kibana.

- **Statsd** is used to collect metric from various sources and it runs on each node as the daemon service.

- **Kibana** is used to aggregate metrics and run on the system with csm service.

Statsd Configuration
--------------------
Run the below mentioned commands to start and enable the **statsd** service. This must be performed on every node.

 ::

  systemctl start statsd

  systemctl enable statsd

To know the status of the service, run the following command.

 ::

  systemctl status statsd

Kibana Configuration
--------------------
1. Update the **kibana.service** file on each system. By default, the service is not compatible with new systemd. Run the following command to check the compatibility.

    ::

     systemd-analyze verify /etc/systemd/system/kibana.service

  If the above command gives a warning, replace the file with **/etc/systemd/system/kibana.service**.

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

    ::

     systemctl daemon-reload

3. Find the active csm service (active node) by running the following command.

    ::

     systemctl status csm_agent

4. Start kibana on the active CSM node and enable the service by running the following commands.

    ::

     systemctl start kibana

     systemctl enable kibana

Check the systemd status on active CSM node by running the following command.

 ::

  systemctl status kibana
 
***************************
Configuration of Components
***************************

Configuration of different components that are part of CORTX are mentioned in the sections below.

S3 (AuthServer and HAProxy)
===========================

AuthServer
----------

The AuthServer is configured along with the installation of S3 component.

Starting Service
^^^^^^^^^^^^^^^^^

- Run the below mentioned command to start the AuthServer.
    
   ::
   
    systemctl start s3authserver

- Run the below mentioned command to restart the AuthServer.

   ::
    
    systemctl restart s3authserver
 
- Run the following command to check the status of AuthServer.

   ::

    systemctl status s3authserver

HAProxy
--------
This section provides information on the installation and configuration associated with HAProxy.

Installation
^^^^^^^^^^^^^

1. Navigate to **/opt/seagate/cortx/s3/install/haproxy**.

2. Copy the contents of **haproxy_osver7.cfg** (**haproxy_osver8.cfg** depending on your OS version) to **/etc/haproxy/haproxy.cfg**.

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
 
From the above result, it can be seen that each node has 4 s3server instances. Hence, each HAProxy will be configured with 4 (s3 instances) x 3 (nodes) = 12 S3 instances in the HAProxy’s  **backend** section of app-main. Let us consider this value of number of S3 instances per node as **N**. There are two procedures for HAproxy configuration, one without external load balancer and the other with external load balancer.

Perform the steps mentioned below to configure HAProxy if external load balancer (DNS RR) is not available.

1. Open **/etc/haproxy/haproxy.cfg** from the active node, and navigate to the **backend app-main** section.

2. Navigate to **backend app-main** section in haproxy.cfg, and locate S3 instance - **server s3-instance-1 0.0.0.0:28081 check maxconn 110**. Then, replace the 0.0.0.0 of all instances with the public data IP addresses  of the current node.

3. Add N – 1 (4 – 1 = 3 for this case) like instances below this. In case of VM, if the number of S3 instances per node is 1, then this step and steps 7 & 8 must be skipped.

4. Keep the instance name (s3-instance-x) for each instance unique, increment x by 1 with increase in instance.

5. Increment the port number (28081) for the next 3 instances by 1. Repeat these steps for nodes 2 & 3 as explained in the next two steps.

6. Copy the above **N** edited instances and paste it below. Change the IP address of these instances to the IP of Node 2. Then, keep the instance name (s3-instance-x) for each instance unique, incrementing x by 1.

7. Repeat the previous step while replacing the IP with the IP for Node – 3 and keeping the instance names unique.

8. Navigate to the **backend s3-auth** section and locate S3 auth instance: **server s3authserver-instance1 0.0.0.0:9085**.  Replace 0.0.0.0 with the public data IP address of current node

9. Add 2 more similar instances below this and replace the IP addresses of those 2 instances with the public data IP addresses of the 2 passive nodes. Keep the s3authserver-instanceX instance ID unique.

10. Comment out the **HAProxy Monitoring Config** section if present.

11. Copy the **haproxy.cfg** to the other server nodes at the same location - **/etc/haproxy/haproxy.cfg**.

12. Configure haproxy logs on all the nodes by running the following commands.

    ::

     mkdir /etc/haproxy/errors/

     cp /opt/seagate/cortx/s3/install/haproxy/503.http /etc/haproxy/errors/

     cp /opt/seagate/cortx/s3/install/haproxy/logrotate/haproxy /etc/logrotate.d/haproxy 

     cp /opt/seagate/cortx/s3/install/haproxy/rsyslog.d/haproxy.conf /etc/rsyslog.d/haproxy.conf

     rm -rf /etc/cron.daily/logrotate

     cp /opt/seagate/cortx/s3/install/haproxy/logrotate/logrotate /etc/cron.hourly/logrotate 

     systemctl restart rsyslog

     systemctl restart haproxy 

     systemctl status haproxy

Perform the steps mentioned below to configure HAProxy if external load balancer (DNS RR) is available. 

1. Open **/etc/haproxy/haproxy.cfg** from the active node, and navigate to the **backend app-main** section.

2. Locate the S3 instance - **server s3-instance-1 0.0.0.0:28081 check maxconn 110**. Add **N – 1**. In case of VM, if the number of S3 instances per node is 1, then three steps (2,3,4) including this will be skipped.

3. Name instances uniquely **(s3-instance-x)** and increment **x** by 1, for every instance.

4. Increment the port number (**28081**) for the next 3 instances, by 1. 

5. Navigate to **backend s3-auth** section, and comment out the **HAProxy Monitoring Config** section if present.

6. Copy the **haproxy.cfg** to the other server nodes at the same location - **/etc/haproxy/haproxy.cfg**. 

7. Configure haproxy logs on all the nodes by running the following commands.

    ::

     mkdir /etc/haproxy/errors/

     cp /opt/seagate/cortx/s3/install/haproxy/503.http /etc/haproxy/errors/

     cp /opt/seagate/cortx/s3/install/haproxy/logrotate/haproxy /etc/logrotate.d/haproxy 

     cp /opt/seagate/cortx/s3/install/haproxy/rsyslog.d/haproxy.conf /etc/rsyslog.d/haproxy.conf

     rm -rf /etc/cron.daily/logrotate

     cp /opt/seagate/cortx/s3/install/haproxy/logrotate/logrotate /etc/cron.hourly/logrotate 

     systemctl restart rsyslog

     systemctl restart haproxy 

     systemctl status haproxy
 
Starting Service
^^^^^^^^^^^^^^^^^
 
- Run the below mentioned command to start the HAProxy services.

   ::
   
    systemctl start haproxy
 
- Run the below mentioned command to check the status of HAProxy services.

   ::
   
    systemctl status haproxy

SSPL
====

The prerequisites and different procedures associated with the configuration of SSPL component are mentioned below.

Initial Steps
--------------

- Run the below mentioned command to ensure that RabbitMQ server and SSPL rpms are installed.

  ::
  
   rpm -qa | grep -E "cortx|rabbitmq" 
   cortx-libsspl_sec-xxxxxxxxxxxxxxxxxxxxx 
   cortx-sspl-xxxxxxxxxxxxxxxxxxxxx 
   cortx-libsspl_sec-method_none-xxxxxxxxxxxxxxxxxxxxx 
   cortx-sspl-test-xxxxxxxxxxxxxxxxxxxxx 
   cortx-prvsnr-cli-xxxxxxxxxxxxxxxxxxxxx 
   cortx-prvsnr-xxxxxxxxxxxxxxxxxxxxx 
   cortx-py-utils-xxxxxxxxxxxxxxxxxxxxx rabbitmq-server-xxxxxxxxxxxxxxxxxxxxx
   
- Run the below mentioned command to ensure that the RabbitMq-server is running and active.

   ::
   
    systemctl status rabbitmq-server

- Run the below mentioned command to ensure that the consul agent is running.

   ::

    ps -aux | grep "consul"
 
Configuration
-------------
Run the below mentioned commands to configure SSPL.

 ::
 
  /opt/seagate/cortx/sspl/bin/sspl_setup post_install -p LDR_R1

  /opt/seagate/cortx/sspl/bin/sspl_setup init -r cortx

  /opt/seagate/cortx/sspl/bin/sspl_setup config -f


Starting Service
-----------------
- Run the following to start the SSPL service.

   ::

    systemctl start sspl-ll

- Run the following to restart the SSPL service.

   ::
   
    systemctl restart sspl-ll**

Run the following command to know the status of the SSPL service.

 ::
 
  systemctl status sspl-ll -l
 
Verification
------------
Perform sanity test and ensure that the SSPL configuration is accurate. Run the following commands to perform the test.

 ::

  /opt/seagate/cortx/sspl/bin/sspl_setup check
  
  /opt/seagate/cortx/sspl/bin/sspl_setup test self
 
CSM
===

The prerequisites and different procedures associated with the configuration of CSM component are mentioned below.

Configuration
-------------

Execute the below mentioned commands on the where CSM service would run after fresh installation.

::

 csm_setup post_install

 csm_setup config

 csm_setup init

You can fine tune the configuration by manually editing the configuration files in **/etc/csm**.


Starting Services
------------------
The starting of services procedure must be performed on only one node.

1. Run the below mentioned commands to start and enable the **csm agent**.

   ::

    systemctl start csm_agent

    systemctl enable csm_agent

2. Run the below mentioned commands to start and enable the **csm web**.

   ::

    systemctl start csm_web

    systemctl enable csm_web

Ensure that the services have started successfully by running the following command.

 :: 
 
  systemctl status <service name>


**Note**: After all the services have started running, the CSM web UI is available at port 28100. Navigate to **https://<IP address of the box>:28100** to access the port.

HA 
==

The prerequisites and different procedures associated with the configuration of HA component are mentioned below.

Prerequisites
-------------

- Installation type identification with provisioner api

 ::

  provisioner get_setup_info

  {'nodes': 1, 'servers_per_node': 2, 'storage_type': '5u84', 'server_type': 'virtual'}
  
Configuration
--------------
To check dependency and configure **HA**, perform **post_install**, **config**, and **init**.

::

 /opt/seagate/cortx/ha/conf/script/ha_setup post_install

 /opt/seagate/cortx/ha/conf/script/ha_setup config

 /opt/seagate/cortx/ha/conf/script/ha_setup init

**********************
 Stopping of Services
**********************
 
 To perform stopping of services refer, `Stopping Services <https://github.com/Seagate/cortx/blob/JBOD-Updates/doc/Stopping%20Services.rst>`_.


