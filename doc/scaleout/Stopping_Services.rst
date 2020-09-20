=================
Stopping Services
=================

Run the below mentioned commands in the order of listing to stop the relevant services.

- CSM
  
  As you know the node on which CSM is running, run the following commands.

  ::
     
   salt '<Node Name>' cmd.run "systemctl stop csm_web"
   
   salt '<Node Name>' cmd.run "systemctl stop csm_agent"
   
   
- SSPL

  :: 

   salt '*' cmd.run "systemctl stop sspl-ll"
   

- S3 (AuthServer and HAProxy)

  ::

   salt '*' cmd.run "systemctl stop haproxy"
   
   salt '*' cmd.run "systemctl stop s3authserver"

      
- I/O Stack

  ::
 
   hctl shutdown --all
   
   
- RabbitMQ

  ::

   systemctl stop rabbitmq-server
   

- LDAP

  ::

   salt '*' cmd.run "systemctl stop slapd"
   
   
- Statsd and Kibana

  As you know the nodes on which statsd and kibana are running, run the following commands.

  ::
  
   salt '<Node Name>' cmd.run "systemctl stop statsd"
   
   salt '<Node Name>' cmd.run "systemctl stop kibana"
   
- Elasticsearch

  ::
  
   salt '*' cmd.run "systemctl stop elasticsearch"
