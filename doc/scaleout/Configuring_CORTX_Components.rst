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

Perform the steps mentioned below to configure HAProxy, if external load balancer (DNS RR) is not available.

1. Open **/etc/haproxy/haproxy.cfg** from the active node, and navigate to the **backend app-main** section.

2. Navigate to **backend app-main** section in haproxy.cfg, and locate S3 instance - **server s3-instance-1 0.0.0.0:28081 check maxconn 110**. Then, replace the 0.0.0.0 of all instances with the public data IP addresses  of the current node.

3. Add N – 1 (4 – 1 = 3 for this case) like instances below this. In case of VM, if the number of S3 instances per node is 1, then this step and steps 7 & 8 must be skipped.

4. Keep the instance name (s3-instance-x) for each instance unique, increment x by 1 with increase in instance.

5. Increment the port number (28081) for all the instances by 1. Repeat these steps for nodes 2 & 3 as explained in the next two steps.

6. Copy the above **N** edited instances and paste it below. Change the IP address of these instances to the IP of Node 2. Then, keep the instance name (s3-instance-x) for each instance unique, incrementing x by 1.

7. Repeat the previous step while replacing the IP with the IP for Node – 3 and keeping the instance names unique.

8. Navigate to the **backend s3-auth** section and locate S3 auth instance: **server s3authserver-instance1 0.0.0.0:9085**.  Replace 0.0.0.0 with the public data IP address of current node

9. Add 2 more similar instances below this and replace the IP addresses of those 2 instances with the public data IP addresses of the 2 passive nodes. Keep the s3authserver-instanceX instance ID unique.

10. Repeat the above step (Step 9) on two other nodes.

11. Comment out the **HAProxy Monitoring Config** section if present.

12. Copy the **haproxy.cfg** to the other server nodes at the same location - **/etc/haproxy/haproxy.cfg**.

13. Configure haproxy logs on all the nodes by running the following commands.

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

Perform the steps mentioned below to configure HAProxy, if external load balancer (DNS RR) is available. 

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
   
   salt '*' cmd.run "systemctl start haproxy"
 
- Run the below mentioned command to check the status of HAProxy services.

  ::
   
   salt '*' cmd.run "systemctl status haproxy"

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

   salt '*' cmd.run "systemctl start sspl-ll"

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
  
 /opt/seagate/cortx/sspl/bin/sspl_setup test self
 
CSM
===

The various aspects associated with the configuration of CSM component are mentioned below.

Configuration
-------------

Execute the below mentioned commands on the node where Statsd and Kibana services are running.

::

 csm_setup post_install

 csm_setup config

 csm_setup init

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

Ensure that the services have started successfully by running the following command.

:: 
 
 systemctl status <service name>


**Note**: After all the services have started running, the CSM web UI is available at port 28100. Navigate to **https://<IP address of the box>:28100** to access the port.

HA 
==

The prerequisite and the configuration procedure associated with the configuration of HA component is mentioned below.

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

 salt '*' cmd.run "/opt/seagate/cortx/ha/conf/script/ha_setup post_install"

 salt '*' cmd.run "/opt/seagate/cortx/ha/conf/script/ha_setup config"

 salt '*' cmd.run "/opt/seagate/cortx/ha/conf/script/ha_setup init"
