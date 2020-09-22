==================
Starting Services
==================

Run the commands mentioned below to stop the services. Please follow the order of listing.


LDAP
=====

Run the following command to start the service on all the 3 nodes.

::

 salt '*' cmd.run "systemctl start slapd"

RabbitMQ
========

Run the below mentioned command to restart the server.

::

 salt '*' cmd.run "service.restart rabbitmq-server"

Statsd and Kibana
=================

Run the below mentioned commands to start and enable Statsd.

::

 salt '<Node Name>' cmd.run "systemctl start statsd"

 salt '<Node Name>' cmd.run "systemctl status statsd"

Run the below mentioned commands to start and enable Kibana.

::

 systemctl start kibana

 systemctl enable kibana
 
 
S3 (AuthServer  and HAProxy)
============================

Run the below mentioned command to start the AuthServer on all the three nodes.

::

 salt '*' cmd.run "systemctl start s3authserver"


Run the below mentioned command to restart the AuthServer on all the three nodes.

::

 salt '*' cmd.run "systemctl restart s3authserver"

Run the below mentioned command to start the HAProxy services.

::

 salt '*' cmd.run "systemctl start haproxy"

SSPL
====

Run the following to start the SSPL service.

::

 salt '*' cmd.run "systemctl start sspl-ll"

Run the below mentioned command if the SSPL service does not start even after running the above command.

::

 "consul kv put system_information/product cluster"

Run the following to restart the SSPL service.

::

 salt '*' cmd.run "systemctl restart sspl-ll"

CSM
===

Run the below mentioned commands to start and enable the csm agent.

::

 systemctl start csm_agent

 systemctl enable csm_agent

Run the below mentioned commands to start and enable the csm web.

::

 systemctl start csm_web

 systemctl enable csm_web
