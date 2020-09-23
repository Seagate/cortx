After the onboarding is complete, you can perform the actions mentioned below.

#. Run the below mentioned command to check the cluster health.

   ::
  
    hctl status
    
   .. figure:: images/HCTL.PNG
      
      **Scale-out / JBOD Output**
   
   
   .. figure:: images/OVAH.PNG
   
      **OVA Output**
   
   
   
#. Trigger IOs from an IO tool.

#. In the GUI, you can see the performance graph as part of the **Dashboard**. Please note that the triggering of IO is reflected in the graph.

   .. image:: images/PG.PNG

IO on Cluster (**Scale-out** / **JBOD** only)
---------------------------------------------

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
