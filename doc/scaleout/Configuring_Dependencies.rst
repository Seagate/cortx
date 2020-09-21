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

2. Run the below mentioned command on one node. As a result, LDAP is setup on all the 3 nodes.

   ::

    salt '*' cmd.run "/opt/seagate/cortx/s3/install/ldap/setup_ldap.sh --defaultpasswd --skipssl --forceclean"

3. After LDAP is setup on the three nodes, perform **LDAP Replication**. Refer the procedure below.

4. Configure **slapd.log** on all 3 nodes using the commands mentioned below.

   ::

    salt '*' cmd.run "cp /opt/seagate/cortx/s3/install/ldap/rsyslog.d/slapdlog.conf /etc/rsyslog.d/slapdlog.conf" 
 
    salt '*' cmd.run "systemctl restart slapd"

    salt '*' cmd.run "systemctl restart rsyslog"

Starting Service
-----------------

- Run the following command to start the service on all the 3 nodes.

  ::

   salt '*' cmd.run "systemctl start slapd"

Run the following command to check the status of the service.

::

 salt '*' cmd.run "systemctl status slapd"

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

   Replace provider with the hostname or node-id in each olcSyncRepl entry below.

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

   Replace provider with the hostname or node-id in each olcSyncRepl entry below.

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

- Ensure that rabbitmq server, provisioner, and sspl RPMs must be installed.

  ::
  
   rpm -qa | grep -Ei "rabbitmq|sspl|prvsnr" 
   cortx-libsspl_sec-1.0.0xxxxxxxxxxxxxxxxxxxxx 
   cortx-libsspl_sec-method_none-1.0.0xxxxxxxxxxxxxxx 
   cortx-prvsnr-cli-1.0.0xxxxxxxxxxxxxxxxxxx 
   cortx-prvsnr-1.0.0xxxxxxxxxxxxxxxxx 
   cortx-sspl-1.0.0xxxxxxxxxxxxxxxx 
   cortx-sspl-test-1.0.0xxxxxxxxxxxxxxxxxxxxxxxx 
   rabbitmq-server-xxxxxxxxxxxxxxxxxx


- The **erlang.cookie** file must be available. Run the below mentioned commands in the order in which they are listed.

  - Generating the file

    ::

     systemctl start rabbitmq-server
     
     systemctl stop rabbitmq-server
     
  - Checking the existance of the file
  
    ::
    
     ls -l /var/lib/rabbitmq/.erlang.cookie
     
  - To copy the file to all nodes
   
    ::
     
     salt-cp "*" /var/lib/rabbitmq/.erlang.cookie /var/lib/rabbitmq/.erlang.cookie --hard-crash
  

Restarting Service
------------------

- Run the below mentioned command to restart the server.

  ::

   salt '*' cmd.run "service.restart rabbitmq-server"

Run the below mentioned command to know the status.

::

 systemctl status rabbitmq-server -l
 
Configuration
-------------
1. Start the RabbitMQ server.

2. Run the below mentioned command.

   ::
   
    salt '*' cmd.run 'pip.install python-consul bin_env="/usr/bin/pip3"'
    
2. Run the below mentioned commands to setup the RabbitMQ cluster.

   - Setting a single (current) node as cluster
 
   ::
   
    /opt/seagate/cortx/sspl/bin/setup_rabbitmq_cluster
   
   - Setting 3 nodes
 
   ::
   
    /opt/seagate/cortx/sspl/bin/setup_rabbitmq_cluster -n srvnode-1,srvnode-2,srvnode-3
    
**Note**: -n NODES where NODES must be FQDN of the respective nodes and separated by comma. For example, -n ssc-vm-2104,ssc-vm-176 
 
Run the below mentioned command to check the status of the RabbitMQ cluster.

::

 salt '*' cmd.run "rabbitmqctl cluster_status"
 

Statsd and Kibana
=================
This section describes the procedures that must be followed to configure statsd and kibana.

- **Statsd** is used to collect metric from various sources and it runs on each node as the daemon service.

- **Kibana** is used to aggregate metrics and run on the system with csm service.

Statsd Configuration
--------------------
Run the below mentioned commands to start and enable the **statsd** service on one node. Ensure that Kibana and CSM are run on the same node.

::

 salt '<Node Name>' cmd.run "systemctl start statsd"

 salt '<Node Name>' cmd.run "systemctl enable statsd"

To know the status of the service, run the following command.

::

 salt '<Node Name>' cmd.run "systemctl status statsd"

Kibana Configuration
--------------------
1. Update the **kibana.service** file on the node where Statsd is running. By default, the service is not compatible with new systemd. Run the following command to check the compatibility.

   ::

    systemd-analyze verify /etc/systemd/system/kibana.service

If the above command gives a warning, replace the file with **/etc/systemd/system/kibana.service**.

In the orignal kibana.service file, **StartLimitInterval** and **StartLimitBurst** are part of **Unit** section but as per new systemd rule it is part of **Service** section.

::

 [Unit]
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

 [Install] 
 WantedBy=multi-user.target
  
2. Reload the daemon by running the following command.

   ::

    systemctl daemon-reload

3. Start kibana on the node where CSM would be active and enable the service by running the following commands.

   ::

    systemctl start kibana

    systemctl enable kibana

Check the status of Kibana by running the following command.

::

 systemctl status kibana
