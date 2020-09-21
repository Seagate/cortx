=================
Stopping Services
=================

Run the commands mentioned below to stop the services. Please follow the order of listing.

CSM
=====

As you know the node on which CSM is running, run the following commands.
 
.. raw:: html

 <details>
 <summary><a>Click here to view the commands. </a></summary>
   
::
     
 salt '<Node Name>' cmd.run "systemctl stop csm_web"
   
 salt '<Node Name>' cmd.run "systemctl stop csm_agent"
   
.. raw:: html
   
 </details>
   
SSPL
====

.. raw:: html

 <details>
 <summary><a>Click here to view the command. </a></summary>

:: 

 salt '*' cmd.run "systemctl stop sspl-ll"
   
.. raw:: html
   
  </details>
   

S3 (AuthServer and HAProxy)
===========================

.. raw:: html

 <details>
 <summary><a>Click here to view the commands. </a></summary>

::

 salt '*' cmd.run "systemctl stop haproxy"
   
 salt '*' cmd.run "systemctl stop s3authserver"
   
.. raw:: html
   
  </details>

      
I/O Stack
=========

.. raw:: html

 <details>
 <summary><a>Click here to view the command. </a></summary>
 
::
 
 hctl shutdown --all
   
   
.. raw:: html
   
  </details>
   
   
RabbitMQ
========

.. raw:: html

 <details>
 <summary><a>Click here to view the command. </a></summary>
 
::

 salt '*' cmd.run "systemctl stop rabbitmq-server"
 
.. raw:: html
   
 </details>
 
 
LDAP
====

.. raw:: html

 <details>
 <summary><a>Click here to view the command. </a></summary>
    
::

 salt '*' cmd.run "systemctl stop slapd"
 
.. raw:: html
   
 </details>
  
   
Statsd and Kibana
=================

As you know the nodes on which statsd and kibana are running, run the following commands.
  
.. raw:: html

 <details>
 <summary><a>Click here to view the command. </a></summary>

::
  
 salt '<Node Name>' cmd.run "systemctl stop statsd"
   
 salt '<Node Name>' cmd.run "systemctl stop kibana"
   
- Elasticsearch

  ::
  
   salt '*' cmd.run "systemctl stop elasticsearch"
