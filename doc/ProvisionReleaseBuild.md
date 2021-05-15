# Provision Release Build

You will need to complete this [guide](https://github.com/Seagate/cortx/blob/main/doc/Release_Build_Creation.rst) before moving onto the steps below.

### 1.  Change Hostname
   ```sudo hostnamectl set-hostname deploy-test.cortx.com```
   - Please use this hostname to make reduce issues further on in the process.
   - Make sure the hostname is changed by running `hostname -f`
   - Reboot the VM `reboot`

### 2. Disable SElinux

- ```sed -i 's/SELINUX=enforcing/SELINUX=disabled/' /etc/selinux/config```

### 3. Install Provisioner API
   
   - Set repository URL
   ```
   export CORTX_RELEASE_REPO="file:///var/artifacts/0"
   ```   
   - Install Provisioner API and requisite packages
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

   # Cleanup temporary repos
   rm -rf /etc/yum.repos.d/*3rd_party*.repo
   rm -rf /etc/yum.repos.d/*cortx_iso*.repo
   yum clean all
   rm -rf /var/cache/yum/
   rm -rf /etc/pip.conf
   ```
### 4. Verify provisioner version (0.36.0 and above)
   ```provisioner --version```
   
### 5. Create the config.ini file
   `vi ~/config.ini`
   - Paste the code below into the config file replacing your network interface names with ens33,..ens37 and storage disks with /dev/sdc,/dev/sdb
   ```
   [srvnode_default]
   network.data.private_interfaces=ens34, ens35
   network.data.public_interfaces=ens36, ens37
   network.mgmt.interfaces=ens33
   bmc.user=None
   bmc.secret=None
   storage.cvg.0.data_devices=/dev/sdc
   storage.cvg.0.metadata_devices=/dev/sdb
   network.data.private_ip=None

   [srvnode-1]
   hostname=deploy-test.cortx.com
   roles=primary,openldap_server

   [enclosure_default]
   type=other

   [enclosure-1]
   ```
### 6. Bootstrap Node
   ```
    provisioner setup_provisioner srvnode-1:$(hostname -f) \
    --logfile --logfile-filename /var/log/seagate/provisioner/setup.log --source rpm \
    --config-path ~/config.ini --dist-type bundle --target-build ${CORTX_RELEASE_REPO}
   ```
### 7. Prepare Pillar Data
```
provisioner configure_setup ./config.ini 1
salt-call state.apply components.system.config.pillar_encrypt
provisioner confstore_export
```

## Non-Cortx Group: System & 3rd-Party Softwares
### 1. Non-Cortx Group: System & 3rd-Party Softwares

```provisioner deploy_vm --setup-type single --states system```

### 2. Prereq component group

``` provisioner deploy_vm --setup-type single --states prereq ```

## Cortx Group: Utils, IO Path & Control Path

### 1. Utils component

``` provisioner deploy_vm --setup-type single --states utils ```

### 2. IO path component group

``` provisioner deploy_vm --setup-type single --states iopath ```

### 3. Control path component group

``` provisioner deploy_vm --setup-type single --states controlpath ```

## Cortx Group: HA

### 1. HA component group

``` provisioner deploy_vm --setup-type single --states ha ```

## Start cluster (irrespective of number of nodes):

### 1. Execute the following command on primary node to start the cluster:

``` cortx cluster start ```

### 2. Verify Cortx cluster status:

``` hctl status ```

## Disable the Firewall

```
systemctl stop firewalld
systemctl disable firewalld
```


## Usage:

Follow this [guide](https://github.com/Seagate/cortx/blob/main/doc/Preboarding_and_Onboarding.rst) to setup the GUI.
   Then to test your system upload data using this [guide](https://github.com/Seagate/cortx/blob/main/doc/testing_io.rst)



## Tested by:

- May 12, 2021: Christina Ku (christina.ku@seagate.com) on VM "LDRr2 - CentOS 7.8-20210511-221524" with 2 disks.
- Jan 6, 2021: Patrick Hession (patrick.hession@seagate.com) on a Windows laptop running VMWare Workstation 16 Pro.
   

