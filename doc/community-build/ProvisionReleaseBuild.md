# Deploy Cortx Build Stack

This document provides step-by-step instructions to deploy the CORTX stack. After completing the steps provided in this document you can:

  - Deploy all the CORTX components
  - Create and run the CORTX cluster

To know about various CORTX components, see [CORTX Components guide](https://github.com/Seagate/cortx/blob/main/doc/Components.md).

## Prerequisite

- All the prerequisites specified in the [Building the CORTX Environment for Single Node](Building-CORTX-From-Source-for-SingleNode.md) document must be satisfied.
- The CORTX packages must be generated using the steps provided in the [Generate Cortx Build Stack guide](Generate-Cortx-Build-Stack.md).


## Procedure

1. Set management IP using the following command:
   ```
   export LOCAL_IP=`ip -o addr | grep -v -w lo |awk '{print $4}' |head -1 |cut -c -14`
   sed -i '38,84d' /etc/nginx/nginx.conf
   ```
   
2. Append the locally hosted packages directory in /etc/nginx/nginx.conf
   
```bash
cat <<EOF>>/etc/nginx/nginx.conf
server {
   listen *:80;
   server_name 127.0.0.1 ${LOCAL_IP};
   location /0 {
   root /var/artifacts;
   autoindex on;
             }
         }
    }
EOF
```

3. Run the following commands to start nginx service
   ```
   systemctl start nginx
   systemctl enable nginx
   ```
	
4. Run the following commands to allow HTTP traffic:
   ```
   firewall-cmd --permanent --zone=public --add-service=http
   firewall-cmd --reload
   ```

5. Run the following commands:

   ```
   SCRIPT_PATH=/mnt/cortx/scripts
   mv /var/artifacts/0/install-2.0.0-0.sh $SCRIPT_PATH/install.sh
   sed -i '/udx-discovery/d;/uds-pyi/d' $SCRIPT_PATH/install.sh && \
   sed -i 's/trusted-host: cortx-storage.colo.seagate.com/trusted-host: '$LOCAL_IP'/' $SCRIPT_PATH/install.sh && \
   sed -i 's#cortx-storage.colo.seagate.com|file://#cortx-storage.colo.seagate.com|baseurl=file:///#' $SCRIPT_PATH/install.sh && \
   sed -i '269s#yum-config-manager --add-repo "${repo}/3rd_party/" >> "${LOG_FILE}"#yum-config-manager --nogpgcheck --add-repo "${repo}/3rd_party/" >> "${LOG_FILE}"#' $SCRIPT_PATH/install.sh
   ```
   
6. Run the script which performs the following actions:

    - Configures yum repositories based on the TARGET-BUILD URL
    - Installs CORTX packages (RPM) and their dependencies from the configured yum repositories
    - Initializes the command shell environment (cortx_setup)

   ```
   cd $SCRIPT_PATH && curl -O https://raw.githubusercontent.com/Seagate/cortx-prvsnr/main/srv/components/provisioner/scripts/install.sh
   chmod +x *.sh 
   ./install.sh -t http://${LOCAL_IP}/0
   ```

## Factory Manufacturing

   In the factory method server is required to be configured with a certain set of values before applying the changes and packaging the server for shipping.

7. #### Configure Server

   ```bash
   cortx_setup server config --name  srvnode-1 --type VM
   ```

8. #### Configure Network

   ```bash
   cortx_setup network config --transport lnet --mode tcp
   cortx_setup network config --interfaces ens32 --type management
   cortx_setup network config --interfaces ens34 --type data
   cortx_setup network config --interfaces ens35 --type private
   ```
   
   **Note:** configure the network interface as per environment

9. #### Configure Storage

   ```bash
   cortx_setup storage config --name enclosure-1 --type virtual
   cortx_setup storage config --controller virtual --mode primary --ip 127.0.0.1 --port 80 --user 'admin' --password 'admin'
   cortx_setup storage config --controller virtual --mode secondary --ip 127.0.0.1 --port 80 --user 'admin' --password 'admin'
   ```

10. #### Create device partitions with below script and run command:

    ```
    curl -O https://raw.githubusercontent.com/mukul-seagate11/cortx-1/main/doc/community-build/create_partitions.sh
    chmod +x create_partitions.sh
    ./create_partitions.sh
    ```

    ```
    cortx_setup storage config --cvg dgB01 --data-devices /dev/sdb1,/dev/sdb2,/dev/sdb3 --metadata-devices /dev/sdb4
    cortx_setup storage config --cvg dgA01 --data-devices /dev/sdc1,/dev/sdc2,/dev/sdc3 --metadata-devices /dev/sdc4
    ```
   
11. #### Configure Security

    ```bash
    cortx_setup security config --certificate /opt/seagate/cortx/provisioner/srv/components/misc_pkgs/ssl_certs/files/stx.pem
    ```

12. #### Initialize Node

    ```bash
    cortx_setup node initialize
    ```
   
13. #### Finalize Node Configuration

    ```bash
    cortx_setup node finalize
    ```

## Field Deployment
   
14. #### Prepare Node by Configuring Server Identification

    ```bash
    cortx_setup node prepare server --site_id 1 --rack_id 1 --node_id 1
    ```
   
15. #### Configure Network which configures the following details as per environment:

    - DNS server(s)
    - Search domain(s)

    ```bash
    cortx_setup node prepare network --hostname <hostname> --search_domains <search-domains> --dns_servers <dns-servers>
    ```

**DHCP**

If the network configuration is DHCP, run following commands else run static.

   ```bash
   cortx_setup node prepare network --type management
   cortx_setup node prepare network --type data
   cortx_setup node prepare network --type private
   ```

**STATIC**

If the network configuration is static, run following commands else run DHCP.

   ```bash
   cortx_setup node prepare network --type management --ip_address <ip_address> --netmask <netmask> --gateway <gateway>
   cortx_setup node prepare network --type data --ip_address <ip_address> --netmask <netmask> --gateway <gateway>
   cortx_setup node prepare network --type private --ip_address <ip_address> --netmask <netmask> --gateway <gateway>
   ```

16. #### Configure Firewall

Default config File for firewall command will be available at `/opt/seagate/cortx_configs/firewall_config.yaml` which must be passed to config argument:

   ```bash
   cortx_setup node prepare firewall --config yaml:///opt/seagate/cortx_configs/firewall_config.yaml
   ```

17. #### Configure the Network Time Server

   ```bash
   cortx_setup node prepare time --server ntp-b.nist.gov --timezone UTC
   ```
  
18. #### Node Finalize

  **Note:** Cleanup local salt-master/ minion configuration on the node:

   ```bash
   cortx_setup node prepare finalize
   ```
   
19. Run the following commands to clean the temporary repos:
    
    ```bash
    yum clean all
    rm -rf /var/cache/yum/
    rm -rf /var/artifacts/0/{python-deps-1.0.0-0.tar.gz,third-party-1.0.0-0.tar.gz,iso,install-2.0.0-0.sh}
    ```

20. #### Cluster Definition

    **Note:** Before running the cluster create command set the environment variable `CORTX_RELEASE_REPO` with the build URL and the cluster create command assumes the first hostname listed is the primary node.
	
    ```bash
    cortx_setup cluster create deploy-test.cortx.com --name cortx_cluster --site_count 1 --storageset_count 1
    cortx_setup cluster show
    ```

21. #### Define the Storage Set
    The storageset create command requires the logical node names of all the nodes to be added in the storage set. The logical node names are assigned to each node in the factory, and the names can be fetched using the `cluster show` command.
	
    ```
    cortx_setup storageset create --name storage-set1 --count 1
    cortx_setup storageset add node storage-set1 srvnode-1
    cortx_setup storageset add enclosure storage-set1 srvnode-1
    cortx_setup storageset config durability storage-set1 --type sns --data 4 --parity 2 --spare 0
    ```

22. #### Prepare Cluster
    ```bash
    cortx_setup cluster prepare
    ```
    
23. Run the following command to deploy and configure CORTX components:
	
    **Note:** The commands should be run in the same order as listed.
    
  - Foundation:
    ```
    cortx_setup cluster config component --type foundation
    ```

  - IO Path:
    ```
    cortx_setup cluster config component --type iopath
    ```
	
  - Control Path:
    ```
    cortx_setup cluster config component --type controlpath
    ```

  - High Availability Path:
    ```
    cortx_setup cluster config component --type ha
    ```
    
24. Run the following command to start the CORTX cluster:
    ```bash
    cortx cluster start
    ```
   
25. Run the following commands to verify the CORTX cluster status:
    ```bash
    hctl status
    ```

26. After the CORTX cluster is up and running, configure the CORTX GUI using the instruction provided in [CORTX GUI guide](https://github.com/Seagate/cortx/blob/main/doc/Preboarding_and_Onboarding.rst).

27. Create the S3 account and perform the IO operations using the instruction provided in [IO operation in CORTX](https://github.com/Seagate/cortx/blob/main/doc/Performing_IO_Operations_Using_S3Client.rst).

**Note:** If you encounter any issue while following the above steps, see [Troubleshooting guide](https://github.com/Seagate/cortx/blob/main/doc/Troubleshooting.md)


### Troubleshooting:

  - If the install.sh script fails then run the following commands:

    ```
    rm - rf /etc/yum.repos.d/*
    rm -rf /etc/pip.conf
    cd $SCRIPT_PATH && ./install.sh -t http://${LOCAL_IP}/0
    ```

### Tested by:

- Sep 11 2021: Mukul Malhotra (mukul.malhotra@seagate.com) on a Windows laptop running VMWare Workstation 16 Pro.
