###################################
Running Distributed Scale-out CORTX 
###################################
This document details the installation procedures that must be followed to install a set of CORTX servers doing distributed erasure across a set of storage devices.

**Important**: Please note that this is a preview of distributed CORTX doing network erasure and it is not failure resilient. Do not use these instructions to run CORTX for production reasons nor for storing critical data. The official Seagate version of Lyve Drive Rack (LDR) can be used for production reasons as it relies on erasure within the enclosures. This release supports normal S3 IO functioning only. S3 degraded storage is not supported.

**********
Procedure
**********
Perform the below mentioned procedure to setup CORTX on JBOD.

1. Perform the setup of Multi node JBOD by referring to `Multi Node JBOD Setup <Multi_Node_JBOD_Setup.rst>`_.

2. Install the CORTX software by referring to `Installing CORTX <Installing_CORTX_Software.rst>`_.

3. Configure the dependencies by referring to `Configuring Dependencies <Configuring_Dependencies.rst>`_. The dependencies are listed below in the order in which they have to be configured.

   - LDAP
 
     - LDAP Replication
  
   - Rabbit MQ
 
   - Statsd and Kibana

4. Configure the I/O stack by referring to `Configuring I / O Stack <Configuring_IO_Stack.rst>`_.

5. Configure the components of CORTX by referring to `Configuring Components <Configuring_CORTX_Components.rst>`_. The components are listed below in the order in which they have to be configured.

   - S3 (AuthServer and HAProxy)
 
   - SSPL

   - CSM
 
   - HA
 
6. You must now complete the preboarding and onboarding processes. Refer to `Preboarding_and_Onboarding <https://github.com/Seagate/cortx/blob/main/doc/Preboarding_and_Onboarding.rst>`_ to execute the  preboarding and onboarding processes.

**************************
Testing and Health Check
**************************

- To test the IO after completing the onboarding process, refer to `Testing_IO <https://github.com/Seagate/cortx/blob/main/doc/testing_io.rst>`_.

- To check the health status after completing the onboarding process, refer to  `Health_Check <https://github.com/Seagate/cortx/blob/main/doc/checking_health.rst>`_.

**********************
Starting of Services
**********************

To perform starting of services refer to `Starting Services <Starting_Services.rst>`_.

**********************
 Stopping of Services
**********************
 
To perform stopping of services refer to `Stopping Services <Stopping_Services.rst>`_.


Tested by:

- Nov 14, 2020: Sakchai Suntinuraks (sakchai.suntinuraks@seagate.com) using ISO (cortx-1.0.0) file

- Sep 20, 2020: Ashwini Borse (ashwini.borse@seagate.com) using ISO (cortx-1.0.0) file 
 
 
