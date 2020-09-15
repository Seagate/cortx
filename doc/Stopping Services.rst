=================
Stopping Services
=================

Run the below mentioned commands in the order of listing to stop the relevant services.

- HA

 ::

  cortxha cluster stop

  cortxha cluster shutdown

- CSM

  ::

   systemctl stop csm_web

   systemctl stop csm_agent

- S3 (AuthServer and HAProxy)

  ::

   systemctl stop s3authserver

   systemctl stop haproxy
   
- SSPL

  :: 

   systemctl stop sspl-ll
   
- I/O Stack

  ::
 
   hctl shutdown --all
   
- RabbitMQ

  ::

   systemctl stop rabbitmq-server

- LDAP

  ::

   systemctl stop slapd
