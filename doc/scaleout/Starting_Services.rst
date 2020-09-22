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
