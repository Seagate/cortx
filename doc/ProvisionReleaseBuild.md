# Provision Release Build

You will need to complete this [guide](https://github.com/Seagate/cortx/blob/main/doc/Release_Build_Creation.rst) before moving onto the steps below.

### 1.  Change Hostname
   ```sudo hostnamectl set-hostname deploy-test.cortx.com```
   - Please use this hostname to make reduce issues further on in the process.
   - Make sure the hostname is changed by running `hostname -f`
   - Reboot the VM `reboot`

### 2. Install Provisioner API
   
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
### 3. Verify provisioner version (0.36.0 and above)
   ```provisioner --version```
   
### 4. Create the config.ini file
   `vi ~/config.ini`
   - Paste the code below into the config file replacing your network interface names with ens33,..ens37
   ```
   [storage]
   type=other



   [srvnode-1]
   hostname=deploy-test.cortx.com
   network.data.private_ip=None
   network.data.public_interfaces=ens34, ens35
   network.data.private_interfaces=ens36, ens37
   network.mgmt.interfaces=ens33
   bmc.user=None
   bmc.secret=None
   ```
### 5. Run the auto_deploy_vm command
   ```
   provisioner auto_deploy_vm srvnode-1:$(hostname -f) --logfile --logfile-filename\
   /var/log/seagate/provisioner/setup.log --source rpm --config-path\
   ~/config.ini --dist-type bundle --target-build ${CORTX_RELEASE_REPO}
   ```
   
   

