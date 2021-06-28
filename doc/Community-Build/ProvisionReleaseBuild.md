# Deploy Cortx Build Stack

This document provides step-by-step instructions to deploy the CORTX stack. After completing the steps provided in this document you can:

  - Deploy all the CORTX components
  - Create and run the CORTX cluster

To know about various CORTX components, see [CORTX Components guide](https://github.com/Seagate/cortx/blob/main/doc/Components.md).

## Prerequisite

- All the prerequisites specified in the [Building the CORTX Environment for Single Node](Building-CORTX-From-Source-for-SingleNode.md) document must be satisfied.

- The CORTX packages must be generated using the steps provided in the [Generate Cortx Build Stack guide](Generate-Cortx-Build-Stack.md).


## Procedure

1. Set repository URL using the following command:

   ```
   export CORTX_RELEASE_REPO="file:///var/artifacts/0"   
   ```

2. Run the following command to install the CORTX provisioner APIs and required packages:

   ```
   yum install -y yum-utils
   yum-config-manager --add-repo "${CORTX_RELEASE_REPO}/3rd_party/"
   yum-config-manager --add-repo "${CORTX_RELEASE_REPO}/cortx_iso/"

   cat <<EOF >/etc/pip.conf
   [global]
   timeout: 60
   index-url: $CORTX_RELEASE_REPO/python_deps/
   trusted-host: $(echo $CORTX_RELEASE_REPO | awk -F '/' '{print $3}')
   EOF

   # Cortx Pre-requisites
   yum install --nogpgcheck -y java-1.8.0-openjdk-headless
   yum install --nogpgcheck -y python3 cortx-prereq sshpass

   # Pre-reqs for Provisioner
   yum install --nogpgcheck -y python36-m2crypto salt salt-master salt-minion

   # Provisioner API
   yum install --nogpgcheck -y python36-cortx-prvsnr

   # Verify provisioner version (0.36.0 and above)
   provisioner --version
   ```

3. Run the following commands to clean the temporary repos:

   ```
   rm -rf /etc/yum.repos.d/*3rd_party*.repo
   rm -rf /etc/yum.repos.d/*cortx_iso*.repo
   yum clean all
   rm -rf /var/cache/yum/
   rm -rf /etc/pip.conf
   ```

4. Create the config.ini file:

     **Note:** The config.ini file requires you to add the metadata disk, data disk, and NICs, information. Run the following command to find the devices on your node:

   ```
   device_list=$(lsblk -nd -o NAME -e 11|grep -v sda|sed 's|sd|/dev/sd|g'|paste -s -d, -)
   ```

   A. To find the metadata disks value for storage.cvg.0.metadata_devices, run:

      ```  
      echo ${device_list%%,*}
      ```

   B. To find the data disks value for storage.cvg.0.data_devices, run:

      ```
      echo ${device_list#*,}
      ```

   C. To find the interfaces as per zones defined in your VM, run:

      ```
      firewall-cmd --get-active-zones
      ```

   D. Run the following command to create a config.ini file:

      ```
      vi ~/config.ini
      ```

   E. Paste the code below into the config file replacing your network interface names with ens33,ens34, ens35, and storage disks with /dev/sdb,../dev/sdc:
      
      **Note:** The values used in the below code are for example purpose, update the values as per the inputs received from the above steps.

      ```
      [srvnode_default]
      network.data.private_interfaces=ens35
      network.data.public_interfaces=ens34
      network.mgmt.interfaces=ens33
      storage.cvg.0.data_devices=/dev/sdb,/dev/sdc,/dev/sdd,/dev/sde,/dev/sdf,/dev/sdg,/dev/sdh,/dev/sdi
      storage.cvg.0.metadata_devices=/dev/sdj
      network.data.private_ip=192.254.254.254
      storage.durability.sns.data=4
      storage.durability.sns.parity=2
      storage.durability.sns.spare=2
      bmc.user=None
      bmc.secret=None

      [srvnode-1]
      hostname=deploy-test.cortx.com
      roles=primary,openldap_server

      [enclosure_default]
      type=other

      [enclosure-1]
      ```

5. To run the bootstrap Node:

   ```
   provisioner setup_provisioner srvnode-1:$(hostname -f) \
   --logfile --logfile-filename /var/log/seagate/provisioner/setup.log --source rpm \
   --config-path ~/config.ini \ 
   --dist-type bundle --target-build ${CORTX_RELEASE_REPO}
   ```

6. Load the config.ini file data for the single node into pillars using following command:

   ```
   provisioner configure_setup ./config.ini 1
   ```
   **Note:** To know more about pillar data, see [Pillar in SaltStack](https://docs.saltproject.io/en/latest/topics/tutorials/pillar.html).

7. Encrypt the pillar data using following command:

   ```
   salt-call state.apply components.system.config.pillar_encrypt
   ```

8. Load the encrypted pillar data to confstore using following command:

   ```
   provisioner confstore_export
   ```

9. Configure the CORTX system and other software:

   A. To configure the system components, run:

      ```
      provisioner deploy_vm --setup-type single --states system
      ```

   B. To configure other components, run:

      ```
      provisioner deploy_vm --setup-type single --states prereq
      ```

10. Configure the CORTX utils, IO path, and control path:

    A. To configure the CORTX utils components, run:

       ```
       provisioner deploy_vm --setup-type single --states utils
       ```

    B. To configure the CORTX IO path components, run:

       ```
       provisioner deploy_vm --setup-type single --states iopath
       ```

    C. To configure CORTX control path components, run:

       ```
       provisioner deploy_vm --setup-type single --states controlpath
       ```

11. To configure CORTX HA components:

    ```
    provisioner deploy_vm --setup-type single --states ha
    ```

12. Run the following command to start the CORTX cluster:

    ```
    cortx cluster start
    ```

13. Run the following commands to verify the CORTX cluster status:

    ```
    hctl status
    ```
    ![CORTX Cluster](https://github.com/Seagate/cortx/blob/main/doc/images/hctl_status_output.png)

14. Run the following commands to disable and stop the firewall:

    ```
    systemctl disable firewalld
    systemctl stop firewalld
    ```

15. After the CORTX cluster is up and running, configure the CORTX GUI using the instruction provided in [CORTX GUI guide](https://github.com/Seagate/cortx/blob/main/doc/Preboarding_and_Onboarding.rst).

16. Create the S3 account and perform the IO operations using the instruction provided in [IO operation in CORTX](https://github.com/Seagate/cortx/blob/main/doc/Performing_IO_Operations_Using_S3Client.rst).

**Note:** If you encounter any issue while following the above steps, see [Troubleshooting guide](https://github.com/Seagate/cortx/blob/main/doc/Troubleshooting.md)

### Tested by:

- May 24 2021: Mukul Malhotra (mukul.malhotra@seagate.com) on a Windows laptop running VMWare Workstation 16 Pro.
- May 12, 2021: Christina Ku (christina.ku@seagate.com) on VM "LDRr2 - CentOS 7.8-20210511-221524" with 2 disks.
- Jan 6, 2021: Patrick Hession (patrick.hession@seagate.com) on a Windows laptop running VMWare Workstation 16 Pro.
