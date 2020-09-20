
***************************************
I/O Configuration (Motr + HARE + S3)
***************************************
Perform the below mentioned procedure to configure the I/O stack.

1. Update the BE tx parameters by running the below mentioned command. The **/etc/sysconfig/motr** gets configured.

   ::
   
    m0provision config

2. Run the below mentioned command to bootstrap the cluster.

   ::

    hctl bootstrap --mkfs /var/lib/hare/cluster.yaml

   **Note**: This command must be used with **mkfs** only while running it for the first time. 

3. Verify the motr utility m0crate, by creating a sample m0crate workload file and running m0crate workload. Run the below mentioned commands.

   ::

    /opt/seagate/cortx/hare/libexec/m0crate-io-conf > /tmp/m0crate-io.yaml
    
    dd if=/dev/urandom of=/tmp/128M bs=1M count=128

    m0crate -S /tmp/m0crate-io.yaml
    
If you want to shutdown the cluster, run the below mentioned command.

::

 hctl shutdown
 
Run the below mentioned command to start the cluster. This command is applicable if the cluster was shutdown. 

::

 hctl bootstrap –c /var/lib/hare
  

=============
Node Restart
=============

Perform the below mentioned steps to restart (rejoin) a node.

1. Run the below mentioned command to start the consul.

   ::
   
    systemctl start hare-consul-agent
    
2. Run the below mentioned command to start the hax.

   ::
   
    systemctl start hare-hax
    
3. Run the below mentioned command to start the motr confd.

   ::
   
    systemctl start m0d@<confd-fid>
    
- **confd-fid** can be found in the **/var/lib/hare/consul-server-conf/consul-server-conf.json** file and **/etc/sysconfig/m0d@**
    
4. Run the below mentioned command to start the motr io service.

   ::
   
    systemctl start m0d@<motr-ios-fid>
    
- **motr ioservice fid** can be found in the **/var/lib/hare/consul-server-conf/consul-server-conf.json** file
    
5. Run the below mentioned command to start the S3 service.

   ::
   
    systemctl start s3server@<s3server-fid>
    
- **s3server fids** can be found in **/etc/sysconfig**
