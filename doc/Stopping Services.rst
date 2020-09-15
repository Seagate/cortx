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

- AuthServer and HAProxy

  ::

   systemctl stop s3authserver

   systemctl stop haproxy 
