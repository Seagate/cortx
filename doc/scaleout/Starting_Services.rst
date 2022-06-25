==================
Starting Services
==================

Click each service to expand and view the commands that must be run to start the respective services.

.. raw:: html

 <details>
 <summary><a>LDAP</a></summary>

Run the following command to start the service on all the 3 nodes.

::

 salt '*' cmd.run "systemctl start slapd"

.. raw:: html

 </details>

.. raw:: html

 <details>
 <summary><a>RabbitMQ</a></summary>

Run the below mentioned command to restart the server.

::

 salt '*' cmd.run "service.restart rabbitmq-server"

.. raw:: html

 </details>

.. raw:: html

 <details>
 <summary><a>Statsd and Kibana</a></summary>


Run the below mentioned commands to start and enable Statsd.

::

 salt '<Node Name>' cmd.run "systemctl start statsd"

 salt '<Node Name>' cmd.run "systemctl status statsd"

Run the below mentioned commands to start and enable Kibana.

::

 systemctl start kibana

 systemctl enable kibana

.. raw:: html

 </details>

.. raw:: html

 <details>
 <summary><a>S3 (AuthServer  and HAProxy)</a></summary>

Run the below mentioned command to start the AuthServer on all the three nodes.

::

 salt '*' cmd.run "systemctl start s3authserver"


Run the below mentioned command to restart the AuthServer on all the three nodes.

::

 salt '*' cmd.run "systemctl restart s3authserver"

Run the below mentioned command to start the HAProxy services.

::

 salt '*' cmd.run "systemctl start haproxy"

.. raw:: html

 </details>

.. raw:: html

 <details>
 <summary><a>SSPL</a></summary>

Run the following to start the SSPL service.

::

 salt '*' cmd.run "systemctl start sspl-ll"

Run the below mentioned command if the SSPL service does not start even after running the above command.

::

 "consul kv put system_information/product cluster"

Run the following to restart the SSPL service.

::

 salt '*' cmd.run "systemctl restart sspl-ll"

.. raw:: html

</details>

.. raw:: html

 <details>
 <summary><a>CSM</a></summary>

Run the below mentioned commands to start and enable the csm agent.

::

 systemctl start csm_agent

 systemctl enable csm_agent

Run the below mentioned commands to start and enable the csm web.

::

 systemctl start csm_web

 systemctl enable csm_web

.. raw:: html

</details>
