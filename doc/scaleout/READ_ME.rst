###################################
Running Distributed Scale-out CORTX 
###################################
This document details the installation procedures that must be followed to install a set of CORTX servers doing distributed erasure across a set of storage devices.

**Important**: Please note that this is a preview of distributed CORTX doing network erasure and it is not failure resilient. Do not use these instructions to run CORTX for production reasons nor for storing critical data. The official Seagate version of Lyve Drive Rack (LDR) can be used for production reasons as it relies on erasure within the enclosures.

**********
Procedure
**********
Perform the below mentioned procedure to setup CORTX on JBOD.

1. Perform the setup of 3 node JBOD by referring to `3 Node JBOD Setup <https://github.com/Seagate/cortx/blob/main/doc/scaleout/3%20Node%20JBOD%20Setup.rst>`_.

2. Install the CORTX software by referring to `Installing CORTX <https://github.com/Seagate/cortx/blob/main/doc/scaleout/Installing%20CORTX%20Software.rst>`_.

3. Configure the I/O stack by referring to `Configuring I / O Stack <https://github.com/Seagate/cortx/blob/main/doc/scaleout/Configuring%20IO%20Stack.rst>`_.

4. Configure the dependencies by referring to `Configuring Dependencies <https://github.com/Seagate/cortx/blob/main/doc/scaleout/Configuring%20Dependencies.rst>`_. The dependencies are listed below in the order in which they have to be configured.

 - LDAP
 
  - LDAP Replication
  
 - Rabbit MQ
 
 - Statsd and Kibana
 
5. Configure the components of CORTX by referring to `Configuring Components <https://github.com/Seagate/cortx/blob/main/doc/scaleout/Configuring%20CORTX%20Components.rst>`_. The dependencies are listed below in the order in which they have to be configured.

 - S3 (AuthServer and HAProxy)
 
 - SSPL

 - CSM
 
 - HA

**********************
 Stopping of Services
**********************
 
To perform stopping of services refer to `Stopping Services <https://github.com/Seagate/cortx/blob/main/doc/scaleout/Stopping%20Services.rst>`_.
 
 
