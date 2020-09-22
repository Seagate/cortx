***************************
Configuration of Components
***************************

Configuration of different components that are part of CORTX are mentioned in the sections below. Please configure every component in the order in which they are listed.

S3 (AuthServer and HAProxy)
===========================

.. raw:: html

 <details>
 <summary><a>Click here for detailed information. </a></summary>
 

AuthServer
----------

The AuthServer is configured along with the installation of S3 component.

Starting Service
^^^^^^^^^^^^^^^^^

- Run the below mentioned command to start the AuthServer on all the three nodes.
    
  ::
   
   salt '*' cmd.run "systemctl start s3authserver"

- Run the below mentioned command to restart the AuthServer on all the three nodes.

  ::
    
   salt '*' cmd.run "systemctl restart s3authserver"
 
- Run the following command to check the status of AuthServer.

  ::

   salt '*' cmd.run "systemctl status s3authserver"

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

Common Procedure
^^^^^^^^^^^^^^^^^
1. Decide whether you are going to use DNS Round-Robin configuration for load distribution for the cluster, or not.  Both configurations are supported, see steps below.

2. Find out the number of **s3server** instances by running the below mentioned commands.

   ::
   
    $ hctl status | grep s3server | wc -l
    
   For example:
  
   ::
   
    $ hctl status | grep s3server | wc -l
    33
    
     N = 33/3
     
   This is total per cluster, and in this example it is 3-node cluster, so number of instances per node (N) is 11.
  
HAProxy Configuration Without DNS Round-Robin
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Note**: This configuration assumes that S3 clients are connecting to either one of the cluster nodes, and that node is then responsible for distributing the load within the cluster.

Let us assume we are configuring **M**-node cluster.  Also let's assume number of *s3server* instances per node is **N**

1. Open **/etc/haproxy/haproxy.cfg** on Node 1, and navigate to the **backend app-main** section:

   ::
   
    #---------------------------------------------------------------------
    # BackEnd roundrobin as balance algorithm
    ---------------------------------------------------------------------
    backend app-main

2. Locate S3 instance definition line:

   ::
   
    server s3-instance-1 0.0.0.0:28081 check maxconn 110

3. Replace the 0.0.0.0 with the private data IP addresses of the current node.

4. Replicate this line **N** times.  In each line keep increasing **s3-instance-X** number, and port number:

   ::
   
    server s3-instance-1 YOUR.IP.ADDR.HERE:28081 check maxconn 110
    server s3-instance-2 YOUR.IP.ADDR.HERE:28082 check maxconn 110
    server s3-instance-3 YOUR.IP.ADDR.HERE:28083 check maxconn 110
    ...

5. Copy the above **N** edited instances and paste it below. Change the IP address of these instances to the private data IP of Node 2. Then, keep the instance name **s3-instance-X** for each instance unique, incrementing **X** by 1.

   For example, with **N=11**:
   
   ::
   
    server s3-instance-11 YOUR.NODE-1-IP.ADDR.HERE:28091 check maxconn 110
    server s3-instance-12 YOUR.NODE-2-IP.ADDR.HERE:28081 check maxconn 110
    server s3-instance-13 YOUR.NODE-2-IP.ADDR.HERE:28082 check maxconn 110
    ...

6. Repeat the previous step for Node 3.

7. Navigate to the **backend s3-auth** section and locate S3 auth instance: **server s3authserver-instance1 0.0.0.0:9085**.

   ::
   
     #----------------------------------------------------------------------
     # BackEnd roundrobin as balance algorith for s3 auth server
     #----------------------------------------------------------------------
     backend s3-auth
         ...
         server s3authserver-instance1 0.0.0.0:9085
         
   Replace 0.0.0.0 with the private data IP address of Node 1.

8. Create a copy of this line for every node in cluster. That is,  with **M**=3, you need 3 entries total.

9. Update private data IP of nodes in respective lines.

10. Keep the **s3authserver-instanceX** instance ID unique by incrementing **X** = 1,2,3...

11. Comment out the **HAProxy Monitoring Config** section if present (or remove it):

    ::
    
     ##---------------------------------------------------------------------
     ##HAProxy Monitoring Config
     ##---------------------------------------------------------------------
     #listen haproxy3-monitoring
     #    bind *:8080                #Haproxy Monitoring run on port 8080
     #    mode http
     #    option forwardfor
     #    option httpclose
     #    stats enable
     #    stats show-legends
     #    stats refresh 5s
     #    stats uri /stats                             #URL for HAProxy monitoring
     #    stats realm Haproxy\ Statistics
     #    #stats auth howtoforge:howtoforge            #User and Password for login to the monitoring dashboard
     #    #stats admin if TRUE
     #    #default_backend app-main                    #This is optionally for monitoring backend


12. Save and close the **haproxy.cfg** file.

13. Copy this **haproxy.cfg** to the other server nodes at the same location - **/etc/haproxy/haproxy.cfg**.

14. Configure haproxy logs by running the following commands on every node in the cluster.

    ::

     mkdir /etc/haproxy/errors/
     
     cp /opt/seagate/cortx/s3/install/haproxy/503.http /etc/haproxy/errors/
     
     cp /opt/seagate/cortx/s3/install/haproxy/logrotate/haproxy /etc/logrotate.d/haproxy 
     
     cp /opt/seagate/cortx/s3/install/haproxy/rsyslog.d/haproxy.conf /etc/rsyslog.d/haproxy.conf
     
     rm -rf /etc/cron.daily/logrotate
     
     cp /opt/seagate/cortx/s3/install/haproxy/logrotate/logrotate /etc/cron.hourly/logrotate
     
     systemctl restart rsyslog
     
15. Apply haproxy config changes by running the following commands on every node in the cluster:

    ::
    
     systemctl restart haproxy 
     
     systemctl status haproxy
     
HAProxy configuration With DNS Round-Robin
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Note**: Configuring DNS Round-Robin itself is outside the scope of this document.  DNS RR is configured in settings of DNS server in your network.  This section only talks about configuring HAProxy.  This configuration assumes that DNS will map single S3 domain name to multiple IP addresses (all nodes in cluster will be added to that DNS entry), and this will distribute the incoming traffic between cluster nodes.

Perform the steps mentioned below to configure HAProxy with DNS Round-Robin.

1. Open **/etc/haproxy/haproxy.cfg** on Node 1, and navigate to the **backend app-main** section.

   ::
   
    #---------------------------------------------------------------------
    # BackEnd roundrobin as balance algorithm
    #---------------------------------------------------------------------
    backend app-main

2. Locate S3 instance definition line:

   ::
   
    server s3-instance-1 0.0.0.0:28081 check maxconn 110

3. Replicate this line **N** times.  In each line keep increasing **s3-instance-X** number, and port number:

   ::
  
    server s3-instance-1 0.0.0.0:28081 check maxconn 110
    server s3-instance-2 0.0.0.0:28082 check maxconn 110
    server s3-instance-3 0.0.0.0:28083 check maxconn 110
    ...
    
4. Comment out the **HAProxy Monitoring Config** section if present (or remove it):
 
   ::
    
    ##---------------------------------------------------------------------
    ##HAProxy Monitoring Config
    ##---------------------------------------------------------------------
    #listen haproxy3-monitoring
    #    bind *:8080                #Haproxy Monitoring run on port 8080
    #    mode http
    #    option forwardfor
    #    option httpclose
    #    stats enable
    #    stats show-legends
    #    stats refresh 5s
    #    stats uri /stats                             #URL for HAProxy monitoring
    #    stats realm Haproxy\ Statistics
    #    #stats auth howtoforge:howtoforge            #User and Password for login to the monitoring dashboard
    #    #stats admin if TRUE
    #    #default_backend app-main                    #This is optionally for monitoring backend

5. Copy the **haproxy.cfg** to the other server nodes at the same location - **/etc/haproxy/haproxy.cfg**.

6. Configure haproxy logs by running the following commands on every node in the cluster.

   ::
   
    mkdir /etc/haproxy/errors/
    
    cp /opt/seagate/cortx/s3/install/haproxy/503.http /etc/haproxy/errors/
    
    cp /opt/seagate/cortx/s3/install/haproxy/logrotate/haproxy /etc/logrotate.d/haproxy 
    
    cp /opt/seagate/cortx/s3/install/haproxy/rsyslog.d/haproxy.conf /etc/rsyslog.d/haproxy.conf
    
    rm -rf /etc/cron.daily/logrotate
    
    cp /opt/seagate/cortx/s3/install/haproxy/logrotate/logrotate /etc/cron.hourly/logrotate
    
    systemctl restart rsyslog
    
7. Apply haproxy config changes by running the following commands on every node in the cluster:

   ::
   
    systemctl restart haproxy 
    
    systemctl status haproxy


Starting Service
^^^^^^^^^^^^^^^^^
 
- Run the below mentioned command to start the HAProxy services.

  ::
   
   salt '*' cmd.run "systemctl start haproxy"
 
- Run the below mentioned command to check the status of HAProxy services.

  ::
   
   salt '*' cmd.run "systemctl status haproxy"
   
.. raw:: html
   
 </details>   


SSPL
====

.. raw:: html

 <details>
 <summary><a>Click here for detailed information. </a></summary>

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

Run the below mentioned commands.

::

 provisioner pillar_set cluster/srvnode-1/network/data_nw/roaming_ip \"127.0.0.1\"
 
 provisioner pillar_set cluster/srvnode-2/network/data_nw/roaming_ip \"127.0.0.1\"
 
 provisioner pillar_set cluster/srvnode-3/network/data_nw/roaming_ip \"127.0.0.1\"
 
Run the below mentioned commands to configure SSPL.

::
 
 salt '*' state.apply components.sspl.config.commons

 salt '*' cmd.run "/opt/seagate/cortx/sspl/bin/sspl_setup post_install -p LDR_R1"

 salt '*' cmd.run "/opt/seagate/cortx/sspl/bin/sspl_setup config -f"


Starting Service
-----------------
- Run the following to start the SSPL service.

  ::

   salt '*' cmd.run "systemctl start sspl-ll"
   
- Run the below mentioned command if the SSPL service does not start.

  ::
  
   "consul kv put system_information/product cluster"
   
- Run the following to restart the SSPL service.

  ::
   
   salt '*' cmd.run "systemctl restart sspl-ll"

Run the following command to know the status of the SSPL service.

::
 
 salt '*' cmd.run "systemctl status sspl-ll -l"
 
 
Verification
------------
Perform sanity test and ensure that the SSPL configuration is accurate. Run the following commands to perform the test.

::

 /opt/seagate/cortx/sspl/bin/sspl_setup check

 
.. raw:: html
   
 </details>
 
CSM
===

.. raw:: html

 <details>
 <summary><a>Click here for detailed information. </a></summary>

Run the below mentioned command. This is a prerquisite.

::

 salt '*' cmd.run "setfacl -m u:csm:rwx /etc/ssl/stx/stx.pem"
 

Configuration
-------------

Execute the below mentioned commands on the node where Statsd and Kibana services are running.

::

 salt '*' cmd.run "setfacl -m u:csm:rwx /etc/ssl/stx/stx.pem"

 salt '*' cmd.run "csm_setup post_install"

 salt '*' cmd.run "csm_setup config"
 
 salt '*' cmd.run "usermod -a -G prvsnrusers csm"
 
 salt '*' cmd.run "usermod -a -G certs csm"

 salt '*' cmd.run "csm_setup init"

You can fine tune the configuration by manually editing the configuration files in **/etc/csm**.

**Important**: Statsd, Kibana, and CSM services must run on the same node.

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
    
Run the below mentioned command if you come across an error related to starting the CSM web services.

::
      
 setfacl -R -m u:csm:rwx /etc/ssl/stx/

Ensure that the services have started successfully by running the following command.

:: 
 
 systemctl status <service name>


**Note**: After all the services have started running, the CSM web UI is available at port 28100. Navigate to **https://<IP address of the box>:28100** to access the port.

.. raw:: html
   
 </details>

HA 
==

.. raw:: html

 <details>
 <summary><a>Click here for detailed information. </a></summary>

Prerequisites
-------------

- Installation type identification with provisioner api

::

 provisioner get_setup_info

 {'nodes': 1, 'servers_per_node': 3, 'storage_type': 'JBOD', 'server_type': 'physical'}
  
Configuration
--------------
To check dependency and configure **HA**, perform **post_install**, **config**, and **init**.

::

 salt '*' cmd.run "/opt/seagate/cortx/ha/conf/script/ha_setup post_install"

 salt '*' cmd.run "/opt/seagate/cortx/ha/conf/script/ha_setup config"

 salt '*' cmd.run "/opt/seagate/cortx/ha/conf/script/ha_setup init"
 
.. raw:: html
   
 </details>
