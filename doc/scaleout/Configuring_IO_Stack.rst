
***************************************
I/O Configuration (Motr + HARE + S3)
***************************************
Perform the below mentioned procedure to configure the I/O stack.

1. Update the BE tx parameters by running the below mentioned command. The **/etc/sysconfig/motr** gets configured.

   ::
   
    m0provision config

2. Define the SNS pool configuration in the cluster.yaml file. Refer below.

   ::
   
    pools:
      - name: the pool
        type: sns
        data_units: 8  # N
        parity_units: 2  # K
        allowed_failures: { site: 0, rack: 0, encl: 0, ctrl: 0, disk: 2 }
      - name: MD pool
        type: md
        data_units: 1
        parity_units: 1
      - name: DIX pool
        type: dix
        data_units: 1
        parity_units: 1   

3. Run the below mentioned command to bootstrap the cluster.

   ::

    hctl start


4. Verify the motr utility m0crate, by creating a sample m0crate workload file and running m0crate workload. Run the below mentioned commands.

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
