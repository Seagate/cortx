=================
Stopping Services
=================

Run the below mentioned commands in the order of listing to stop the relevant services.

- CSM 

  ::

   systemctl stop csm_web

   systemctl stop csm_agent
   
   If you know the node in which CSM is running, run the following commands.
   
   salt '<Node Name>' systemctl stop csm_web
   
   salt '<Node Name>' systemctl stop csm_web
   
   
- SSPL

  :: 

   salt '*' systemctl stop sspl-ll
   

- S3 (AuthServer and HAProxy)

  ::

   salt '*' systemctl stop haproxy
   
   salt '*' systemctl stop s3authserver

      
- I/O Stack

  ::
 
   hctl shutdown --all
   
   
- RabbitMQ

  ::

   systemctl stop rabbitmq-server
   

- LDAP

  ::

   salt '*' systemctl stop slapd
   
   
- Statsd and Kibana

  ::
  
   systemctl stop statsd
   
   systemctl stop kibana
   
_ Elasticsearch

  ::
  
   salt '*' systemctl stop elasticsearch
