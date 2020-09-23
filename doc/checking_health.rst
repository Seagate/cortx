***********************
CORTX Health via CLI
***********************

This document describes various methods by which a user can monitor and query the health of a CORTX system using a command line interface.

#. The provided *hctl* program can be used for checking CORTX health.

   .. figure:: images/hctl_health.png

#. System status can be shown with the *status* flag.  Note that this image shows a scale-out CORTX cluster with three CORTX servers.

   ::
  
    hctl status
    
   .. figure:: images/HCTL.PNG
   
#. When running a scale-out CORTX system, the following steps can show how IO is distributed across the CORTX servers.

   #. As was shown in the previous step, the *hctl_status* command shows that there are 11 S3 server instances running on each node. 
   
   #. The image below shows the HAProxy config file which shows the mapping from S3 server instance to IP address.  As can be seen, there are three IP addresses representing the three CORTX servers running in this scale-out system.
 
   .. image:: images/HAP.PNG
  
   #. To see IOs occuring across the system, use the below command and the multiple S3 instances receiving IO.  Using the mapping from the previous step, it can be seen which CORTX servers are currently performing IO. 

   ::

    tail -f /var/log/haproxy.log
 
   .. image:: images/AWS.PNG
