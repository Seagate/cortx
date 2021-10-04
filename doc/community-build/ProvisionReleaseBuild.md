# Deploy Cortx Build Stack

This document provides step-by-step instructions to deploy the CORTX stack. After completing the steps provided in this document you can:

  - Deploy all the CORTX components
  - Create and run the CORTX cluster

To know about various CORTX components, see [CORTX Components guide](https://github.com/Seagate/cortx/blob/main/doc/Components.md).

## Prerequisite

- All the prerequisites specified in the [Building the CORTX Environment for Single Node](Building-CORTX-From-Source-for-SingleNode.md) document must be satisfied.
- The CORTX packages must be generated using the steps provided in the [Generate Cortx Build Stack guide](Generate-Cortx-Build-Stack.md).
- Do not update OS or kernel package with `yum update` as the kernel version must be set to `3.10.0-1160.el7`
- Do not upgrade packages from CentOS 7.8 to CentOS 7.9
- Synchronize time on Installed Operating Systems with chronyc or NTP.


## Procedure

1. Set local IP using the following command:

   **Note:** You must use your local interface name i.e. ens32,ens33 etc as per your environment and verify by running `ip l`
   
   ```
   export LOCAL_IP=$(ip -4 addr show ens33 | grep -oP '(?<=inet\s)\d+(\.\d+){3}')
   export SCRIPT_PATH="/mnt/cortx/scripts"
   ```
   
2. Run the following pre-setup script:

   ```
   cd $SCRIPT_PATH; curl -O https://raw.githubusercontent.com/mukul-seagate11/cortx-1/main/doc/community-build/presetup.sh
   sh presetup.sh
   ```

3. Run the following commands:
   ```
   sed -i '/udx-discovery/d;/uds-pyi/d' $SCRIPT_PATH/install.sh
   sed -i 's/trusted-host: cortx-storage.colo.seagate.com/trusted-host: '$LOCAL_IP'/' $SCRIPT_PATH/install.sh
   sed -i 's#cortx-storage.colo.seagate.com|file://#cortx-storage.colo.seagate.com|baseurl=file:///#' $SCRIPT_PATH/install.sh
   sed -i '269s#yum-config-manager --add-repo "${repo}/3rd_party/" >> "${LOG_FILE}"#yum-config-manager --nogpgcheck --add-repo "${repo}/3rd_party/" >> "${LOG_FILE}"#' $SCRIPT_PATH/install.sh
   ```
   
4. Run the following command to test the url accessibility status:

   ```
   curl -X GET http://${LOCAL_IP}/0 && if [ $? -eq 0 ];then echo "SUCCESS"; else echo "FAILED"; fi
   ```
   
5. Run the script as instructed which will performs the following actions:
    - Configures yum repositories based on the TARGET-BUILD URL
    - Installs CORTX packages (RPM) and their dependencies from the configured yum repositories
    - Initializes the command shell environment (cortx_setup)

   ```
   sh install.sh -t http://${LOCAL_IP}/0
   ```

## Factory Manufacturing

   In the factory method server is required to be configured with a certain set of values before applying the changes and packaging the server for shipping.

6. #### Configure Server

   ```bash
   cortx_setup server config --name  srvnode-1 --type VM
   ```

7. #### Configure Network

   **Note:** You must use network interfaces as per your environment as mentioned interfaces are for example

   ```bash
   cortx_setup network config --transport lnet --mode tcp
   cortx_setup network config --interfaces ens33 --type management
   cortx_setup network config --interfaces ens34 --type data
   cortx_setup network config --interfaces ens35 --type private
   ```

8. #### Configure Storage

   ```bash
   cortx_setup storage config --name enclosure-1 --type virtual
   cortx_setup storage config --controller virtual --mode primary --ip 127.0.0.1 --port 80 --user 'admin' --password 'admin'
   cortx_setup storage config --controller virtual --mode secondary --ip 127.0.0.1 --port 80 --user 'admin' --password 'admin'
   ```

9. #### Run the script to create disk partitions and run the `cortx_setup` command:

    ```
    curl -O https://raw.githubusercontent.com/mukul-seagate11/cortx-1/main/doc/community-build/create_partitions.sh
    sh create_partitions.sh
    ```

    ```
    cortx_setup storage config --cvg dgB01 --data-devices /dev/sdb1,/dev/sdb2,/dev/sdb3 --metadata-devices /dev/sdb4
    cortx_setup storage config --cvg dgA01 --data-devices /dev/sdc1,/dev/sdc2,/dev/sdc3 --metadata-devices /dev/sdc4
    ```
   
10. #### Configure Security

    ```bash
    cortx_setup security config --certificate /opt/seagate/cortx/provisioner/srv/components/misc_pkgs/ssl_certs/files/stx.pem
    ```

11. #### Initialize Node

    ```bash
    cortx_setup node initialize
    ```
   
12. #### Finalize Node Configuration

    ```bash
    cortx_setup node finalize
    ```
    
13. Run the following commands to clean the temporary repos:
    
    ```bash
    rm -rf /etc/yum.repos.d/*3rd_party*.repo
    rm -rf /etc/yum.repos.d/*cortx_iso*.repo
    yum clean all
    rm -rf /var/artifacts/0/{python-deps-1.0.0-0.tar.gz,third-party-1.0.0-0.tar.gz,iso,install-2.0.0-0.sh}
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
    
    For example:
    
    ```
    cortx_setup node prepare network --hostname deploy-test.cortx.com --search_domains cortx.com --dns_servers 192.168.220.2
    ```

16. If the network configuration is DHCP, run following commands else run static:

   ```bash
   cortx_setup node prepare network --type management
   cortx_setup node prepare network --type data
   cortx_setup node prepare network --type private
   ```

17. (Optional) If the network configuration is static, run following commands else run DHCP.

   ```bash
   cortx_setup node prepare network --type management --ip_address <ip_address> --netmask <netmask> --gateway <gateway>
   cortx_setup node prepare network --type data --ip_address <ip_address> --netmask <netmask> --gateway <gateway>
   cortx_setup node prepare network --type private --ip_address <ip_address> --netmask <netmask> --gateway <gateway>
   ```

18. #### Configure Firewall

   Default config File for firewall command will be available at `/opt/seagate/cortx_configs/firewall_config.yaml` which must be passed to config argument:

   ```bash
   cortx_setup node prepare firewall --config yaml:///opt/seagate/cortx_configs/firewall_config.yaml
   ```

19. #### Configure the Network Time Server

   ```bash
   cortx_setup node prepare time --server ntp-b.nist.gov --timezone UTC
   ```
  
20. #### Node Finalize

  
   **Note:** Cleanup local salt-master/ minion configuration on the node:

   ```bash
   cortx_setup node prepare finalize
   ```

21. #### Cluster Definition

   **Note:** Enter root password when prompted
	
   ```bash
   cortx_setup cluster create deploy-test.cortx.com --name cortx_cluster --site_count 1 --storageset_count 1
   cortx_setup cluster show
   ```
    
   [![cluster_definition_output.png](https://github.com/Seagate/cortx/blob/main/doc/images/cluster_definition_output.png "cluster_definition_output.png")](https://github.com/Seagate/cortx/blob/main/doc/images/cluster_definition_output.png "cluster_definition_output.png")

22. #### Define the Storage Set

    The storageset create command requires the logical node names of all the nodes to be added in the storage set. The logical node names are assigned to each node in the factory, and the names can be fetched using the `cluster show` command.
	
    ```
    cortx_setup storageset create --name storage-set1 --count 1
    cortx_setup storageset add node storage-set1 srvnode-1
    cortx_setup storageset add enclosure storage-set1 srvnode-1
    cortx_setup storageset config durability storage-set1 --type sns --data 4 --parity 2 --spare 0
    ```

23. #### Prepare Cluster
    ```bash
    cortx_setup cluster prepare
    ```
    
24. Run the following command to deploy and configure CORTX components:
	
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
    
25. Run the following commands to stop the nginx service:

    ```
    systemctl stop nginx
    systemctl disable nginx
    ```
    
26. Run the following command to start the CORTX cluster:
    ```bash
    cortx cluster start
    ```
   
27. Run the following commands to verify the CORTX cluster status:
    ```bash
    hctl status
    ```
    
    [![hctl_status](https://github.com/Seagate/cortx/blob/main/doc/images/hctl_status.PNG "hctl_status")](https://github.com/Seagate/cortx/blob/main/doc/images/hctl_status.PNG "hctl_status")

28. After the CORTX cluster is up and running, configure the CORTX GUI using the instruction provided in [CORTX GUI guide](https://github.com/Seagate/cortx/blob/main/doc/community-build/Preboarding_and_Onboarding.rst).

29. Create the S3 account and perform the IO operations using the instruction provided in [IO operation in CORTX](https://github.com/Seagate/cortx/blob/main/doc/Performing_IO_Operations_Using_S3Client.rst).


## Troubleshooting

 - If you encounter any issue while following the above steps, see [Troubleshooting guide](https://github.com/Seagate/cortx/blob/main/doc/community-build/Troubleshooting.md)

## Known Issues

 - See the [Known Issues](https://github.com/Seagate/cortx/blob/main/doc/community-build/CHANGELOG.md) for more details.



### Tested by:

- Sep 11 2021: Mukul Malhotra (mukul.malhotra@seagate.com) on a Windows laptop running VMWare Workstation 16 Pro for CentOS 7.9.2009
