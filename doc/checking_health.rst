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
   

The S3instance assignment per node is as follows.

::

 s3intance-1  to s3intance-11 =====> srvnode-1
 
 s3intance-12 to s3intance-22 =====> srvnode-2
 
 s3intance-23 to s3intance-33 =====> srvnode-3
 
The image below depicts the HAProxy config.
 
.. image:: images/HAP.PNG
  
On node 1, run the below mentioned command when IOs are started, to check if IOs are running on all nodes.

::

 tail -f /var/log/haproxy.log
 
.. image:: images/AWS.PNG

.. raw:: html
