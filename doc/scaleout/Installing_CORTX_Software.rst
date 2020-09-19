******************************
Installation of CORTX Software
******************************

This section provides information on the installation of Provisioner and the associated API. Perform the below mentioned procedure to complete the process of installation.

1. Login to the first server of the cluster that you want to install and become root.

2. Start the **screen** or **tmux** session to avoid the stalling of installation. If these utilities are not present, install them using CentOS tools (yum).

3. Run the below mentioned command to install the CORTX Provisioner API.

   ::

    pip install https://github.com/Seagate/provisioner-test/releases/download/cortx-api-v0.33.0/cortx-prvsnr-0.33.0.tar.gz
    
**Note**: If you installed Python 3.6 without the virtual environment, replace **pip** command with **pip3**.

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
    
**Note**: You will be prompted for the root password of each server.

5. Run the below mentioned commands to verify that the dependency components are installed successfully.

   :: 
 
    /usr/share/kibana/bin/kibana --version
    
    slapd -V

    /usr/share/elasticsearch/bin/elasticsearch --version**

    rabbitmqadmin --version

    node --version

    lfs --version

The output of these commands should match the following:

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
