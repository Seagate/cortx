=================
Stopping Services
=================

Run the below mentioned commands in the order of listing to stop the relevant services.

- CSM

  ::

   systemctl stop csm_web

   systemctl stop csm_agent
   
   
- SSPL

  :: 

   systemctl stop sspl-ll
   

- S3 (AuthServer and HAProxy)

  ::

   systemctl stop s3authserver

   systemctl stop haproxy
   
     
- I/O Stack

  ::
 
   hctl shutdown --all
   
   
- RabbitMQ

  ::

   systemctl stop rabbitmq-server
   

- LDAP

  ::

   systemctl stop slapd
