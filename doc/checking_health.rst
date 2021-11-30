***********************
CORTX Health via CLI
***********************

This document describes various methods by which a user can monitor and query the health of a CORTX system using a command line interface.

#. The provided *hctl* program can be used for checking CORTX health.

   .. figure:: images/hctl_health.png

#. The remainder of this document uses a working example with an S3 client attached to a three node CORTX cluster as is depicted in the below image.

   .. figure:: images/s3_three_cortx_nodes.png

#. System status can be shown with the *status* flag.  

   ::
  
    hctl status
    
   .. figure:: images/HCTL.PNG
   
#. When running a scale-out CORTX system, the following steps can show how IO is distributed across the CORTX servers.

   #. As was shown in the previous step, the *hctl_status* command shows that there are 11 S3 server instances running on each node. 
   
   #. The image below shows the HAProxy config file which shows the mapping from S3 server instance to IP address.  As can be seen, there are three IP addresses representing the three CORTX servers running in this scale-out system. The path of the config is **/etc/haproxy/haproxy.cfg**.
 
   .. image:: images/HAP.PNG
  
   #. To see IOs occuring across the system, use the below command and the multiple S3 instances receiving IO.  Using the mapping from the previous step, it can be seen which CORTX servers are currently performing IO. 

   ::

    tail -f /var/log/haproxy.log
 
   .. image:: images/AWS.PNG
   
   #. The *dstat* command can also show IO activity.  The below figure shows dstat output from the three servers when someone first *put* an object, then did *get* on that same object and then finally deleted the object.
      
   
   .. image:: images/dstat_view.png
   
   
   As can be seen, even though the S3 client communicates with a single CORTX server, CORTX distributes data and parity blocks across all 3 servers.


