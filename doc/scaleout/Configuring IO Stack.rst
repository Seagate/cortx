
***************************************
I/O Configuration (Motr + HARE + S3) 
***************************************
Perform the below mentioned procedure to configure the I/O stack.

1. Update the BE tx parameters by running the below mentioned command. The **/etc/sysconfig/motr** gets configured.

   ::
    
    m0provision config

2. Run the below mentioned command to bootstrap the cluster.

   ::

    hctl bootstrap --mkfs cluster.yaml

  This command must be used with **mkfs** only while running it for the first time. 

3. Verify the motr utility m0crate, by creating a sample m0crate workload file and running m0crate workload. Run the below mentioned commands.

   ::

    /opt/seagate/cortx/hare/libexec/m0crate-io-conf > /tmp/m0crate-io.yaml

    m0crate -S /tmp/m0crate-io.yaml

Run the below mentioned command to start the cluster. This command must be used while starting the cluster from second time.

 ::

  hctl bootstrap –c /var/lib/hare
  
