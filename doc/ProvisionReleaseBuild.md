# Deploy Cortx Stack Build

You will need to complete this [guide](https://github.com/Seagate/cortx/blob/main/doc/Release_Build_Creation.rst) before moving onto the steps below.

### Pre-requisite

- Change Hostname by running,
   ```
   sudo hostnamectl set-hostname deploy-test.cortx.com
   ```
   - Please use this hostname to avoid issues further in the bootstrap process.
   - Make sure the hostname is changed by running `hostname -f`
 
- Disable SElinux by running,
   ```
   sed -i 's/SELINUX=enforcing/SELINUX=disabled/' /etc/selinux/config
   ```
- Set repository URL
   ```
   export CORTX_RELEASE_REPO="file:///var/artifacts/0"
   ```   
- Reboot your VM by running `reboot` command

- Cleanup temporary repos
   ```
    rm -rf /etc/yum.repos.d/*3rd_party*.repo
    rm -rf /etc/yum.repos.d/*cortx_iso*.repo
    yum clean all
    rm -rf /var/cache/yum/
    rm -rf /etc/pip.conf
   ```

## Procedure for VM Deployment Steps

### 1. Install Provisioner API and requisite packages
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
   ```

### 2. Verify provisioner version (0.36.0 and above)
    provisioner --version
   
### 3. Create the config.ini file

*Note:* You can find the devices on your node by running below command to update in config.ini
    
    device_list=$(lsblk -nd -o NAME -e 11|grep -v sda|sed 's|sd|/dev/sd|g'|paste -s -d, -)

  - Values for storage.cvg.0.metadata_devices:
   ```
    echo ${device_list%%,*}
   ```
  - Values for storage.cvg.0.data_devices:
   ``` 
    echo ${device_list#*,}
   ```
  - You can find the interfaces as per zones defined in your setup by running,
   ```
    firewall-cmd --get-active-zones
   ```
   
    vi ~/config.ini
    
   - Paste the code below into the config file replacing your network interface names with ens33,..ens35 and storage disks with /dev/sdb,../dev/sdc
   ```
   [srvnode_default]
   network.data.private_interfaces=ens35
   network.data.public_interfaces=ens34
   network.mgmt.interfaces=ens33
   bmc.user=None
   bmc.secret=None
   
   #data devices
   storage.cvg.0.data_devices=/dev/sdc
   
   #metadata devices
   storage.cvg.0.metadata_devices=/dev/sdb
   network.data.private_ip=None

   [srvnode-1]
   hostname=deploy-test.cortx.com
   roles=primary,openldap_server

   [enclosure_default]
   type=other

   [enclosure-1]
   ```
### 4. Bootstrap Node
   ```
    provisioner setup_provisioner srvnode-1:$(hostname -f) \
    --logfile --logfile-filename /var/log/seagate/provisioner/setup.log --source rpm \
    --config-path ~/config.ini --dist-type bundle --target-build ${CORTX_RELEASE_REPO}
   ```
### 5. Prepare Pillar Data

Update data from config.ini into Salt pillar. Export pillar data to provisioner_cluster.json
   ```
    provisioner configure_setup ./config.ini 1
    salt-call state.apply components.system.config.pillar_encrypt
    provisioner confstore_export
   ```

## Non-Cortx Group: System & 3rd-Party Softwares

- ### System components:

   ```
   provisioner deploy_vm --setup-type single --states system
   ```

- ### 3rd-Party components:

   ```
   provisioner deploy_vm --setup-type single --states prereq
   ```

## Cortx Group: Utils, IO Path & Control Path

- ### Utils component:

   ```
   provisioner deploy_vm --setup-type single --states utils
   ```

- ### iopath components:

   ```
   provisioner deploy_vm --setup-type single --states iopath
   ```

- ### Controlpath components:

   ```
   provisioner deploy_vm --setup-type single --states controlpath
   ```

## Cortx Group: HA

- ### HA components:

   ```
   provisioner deploy_vm --setup-type single --states ha
   ```

## Start cortx cluster (irrespective of number of nodes):

- ### Execute the following command on primary node to start the cluster:

   ```
   cortx cluster start
   ```

- ### Verify cortx cluster status:

   ```
   hctl status
   ```

## Usage:

Follow this [guide](https://github.com/Seagate/cortx/blob/main/doc/Preboarding_and_Onboarding.rst) to setup the GUI.
   Then to test your system upload data using this [guide](https://github.com/Seagate/cortx/blob/main/doc/testing_io.rst)



## Tested by:

- May 20 2021: Mukul Malhotra (mukul.malhotra@seagate.com) on a Windows laptop running VMWare Workstation 16 Pro.
- May 12, 2021: Christina Ku (christina.ku@seagate.com) on VM "LDRr2 - CentOS 7.8-20210511-221524" with 2 disks.
- Jan 6, 2021: Patrick Hession (patrick.hession@seagate.com) on a Windows laptop running VMWare Workstation 16 Pro.
