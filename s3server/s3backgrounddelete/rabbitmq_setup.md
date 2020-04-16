## Steps to install & configure RabbitMQ

1) Install rabbitmq (On all nodes)
    Ref: https://www.rabbitmq.com/install-rpm.html#package-cloud
    Ref:https://packagecloud.io/rabbitmq/rabbitmq-server/install#bash-rpm


   >curl -s https://packagecloud.io/install/repositories/rabbitmq/rabbitmq-server/script.rpm.sh | sudo bash

2) Install pika on all nodes( BSD 3-Clause license)

    CentOS 7:
   > yum -y install  python36-pika

    RHEL/CentOS 8:
   > yum -y localinstall https://dl.fedoraproject.org/pub/epel/8/Everything/x86_64/Packages/p/python3-pika

3) Setup hosts file (/etc/hosts) on each node, so that nodes are
   accessible from each other
----
    cat /etc/hosts
    127.0.0.1   localhost localhost.localdomain localhost4 localhost4.localdomain4
    ::1         localhost localhost.localdomain localhost6 localhost6.localdomain6
    192.168.64.168 <hostname_1>
    192.168.64.170 <hostname_2>
----

4) Start rabbitmq (On all nodes)

   >systemctl start rabbitmq-server

5) Automatically start RabbitMQ at boot time 9On all nodes)

   >systemctl enable rabbitmq-server

6) Open up some ports, if we have firewall running (On all nodes)


   >firewall-cmd --zone=public --permanent --add-port=4369/tcp

   >firewall-cmd --zone=public --permanent --add-port=25672/tcp

   >firewall-cmd --zone=public --permanent --add-port=5671-5672/tcp

   >firewall-cmd --zone=public --permanent --add-port=15672/tcp

   >firewall-cmd --zone=public --permanent --add-port=61613-61614/tcp

   >firewall-cmd --zone=public --permanent --add-port=1883/tcp

   >firewall-cmd --zone=public --permanent --add-port=8883/tcp

   Reload the firewall

   >firewall-cmd --reload

7) Enable RabbitMQ Web Management Console (On all nodes)

   >rabbitmq-plugins enable rabbitmq_management

8) Setup RabbitMQ Cluster (Assuming there are 2 nodes 'hostname_1' and 'hostname_2')

    In order to setup the RabbitMQ cluster, we need to make sure the '.erlang.cookie' file is same on all nodes.

    We will copy the '.erlang.cookie' file in the '/var/lib/rabbitmq' directory from 'hostname_1' to other node
    'hostname_2'.


    Copy the '.erlang.cookie' file using scp commands from the 'hostname_1' node.

   >scp /var/lib/rabbitmq/.erlang.cookie root@'hostname_2':/var/lib/rabbitmq/


9) Run below commands from 'hostname_2' node (node that will join 'hostname_1' node)

   >systemctl restart rabbitmq-server
   >rabbitmqctl stop_app

    Let RabbitMQ server on 'hostname_2' node, join 'hostname_1' node

   >rabbitmqctl join_cluster rabbit@'hostname_1'
   >rabbitmqctl start_app

    check the RabbitMQ cluster status on both nodes

   >rabbitmqctl cluster_status


10) Create a user (In this case user is eos-s3 with password as password)

    >rabbitmqctl add_user eos-s3 password


11) Setup the eos-s3 user as an administrator.

    >rabbitmqctl set_user_tags eos-s3 administrator


11) Set permission

    >rabbitmqctl set_permissions -p / eos-s3 ".*" ".*" ".*"


12) Check users on cluster nodes (all nodes will list them)
    >rabbitmqctl list_users


13) Setup queue mirroring

    >rabbitmqctl set_policy ha-all ".*" '{"ha-mode":"all"}'


14) Tests
    Open your web browser and type the IP address of the node with port '15672'.

    >http://192.168.64.170:15672/

    Type username 'eos-s3' with password 'password'

    Note:Ensure that port 15672 is open
