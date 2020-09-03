==========================
Cortx on Virtual Appliance
==========================
Virtual Appliance (VA) is a virtual machine image file that consists of pre-installed and pre-configured operating system environment, and a single application.

This document provides information on the procedure that must be followed to install Cortx (all components included) on a VA.

**************
Prerequisites
**************
The prerequisites required to install Cortx on a VA is listed below.

- GitHub Access
- Unzipping Software (preferably WinZip)
- Any of the following:
 - VMware ESX server
 - VMware ESXi server
 - VMware Workstation
 - VMware Fusion

**********
Procedure
**********
The procedure to install Cortx on VA is mentioned below.

1. Login to `GitHub <https://github.com/>`_ with your credentials.
2. Select the relevant repository, and navigate to the appropriate directory. *<<we need to give links of the repository and directory>>*

3. From the directory, download the ZIP file that contains the VMware virtual machine images.

4. Extract the contents of the downloaded ZIP file into your system.

5. Install the VM on your system by referring `Deploying a VM <https://github.com/Seagate/cortx/blob/main/doc/Deploying%20a%20VM.rst>`_.

 - In case of troubleshooting, refer `VM Documents <https://docs.vmware.com/en/VMware-vSphere/index.html>`_.
 
6. Turn on the virtual machine and login using either the SSH or VMware console.

 - Username: cortx
 - Password: opensource!
 
7. As the super user, run the following command:

 - **sudo su -**
 
8. Update the hostname by running the below mentioned command. By default the name is set to **localhost.localdomain**.

 - **hostnamectl set-hostname --static --transient --pretty <new_hostname>**

     If you receive **Access denied** message, remove immutable settings on the **/etc/hostname** file and run the command again. To remove immutable setting from **/etc/hostname**, run the following command.
     
     - **chattr -i /etc/hostname**
     
  **Note**: Both short hostnames and FQDNs are accepted. If you do not have DNS server to register the VM with, you can access it using the IP address. However, the hostname is mandatory and should be configured.
  
9. Update **/etc/hosts** with the management IP address and the new hostname for the VA.

10. Edit **/root/.ssh/config** and update the following with the new hostname for the VA.

  - **Host srvnode-1 <new_hostname>**
  
  - **HostName <new_hostname>**
  
  **Note**: Please keep **srvnode-1** in the Host field. This is an internal name and it's required for the proper functioning of VA.

11. Refresh HAproxy configuration by running the following command.

  - **salt "*" saltutil.pillar_refresh**
  
  - **salt "*" state.apply components.ha.haproxy.config**
  
  - **salt "*" state.apply components.ha.haproxy.start**
  
12. Restart lnet by running the following command.

  - **systemctl restart lnet**
  

13. Run the following command:

 - **hctl bootstrap --mkfs /var/lib/hare/cluster.yaml**

  You must run the above command with **--mkfs** only once. Further usage of **--mkfs** erases data.

14. Ensure that the I/O stack is running by running the following command:

 - **hctl status**

15. Ensure that the CSM service is operational by running the following commands:

 - **systemctl status csm_agent**
 - **systemctl status csm_web**

   If the above services are not active, run the following command:

  - **systemctl start <csm_agent|csm_web>**
  
16. Open the web browser and navigate to the following location:

  - **https://<management IP>:28100/#/preboarding/welcome**
  
Note: Operating system updates are not supported due to specific kernel dependencies.
  
Restarting CORTX VA
===================
To restart the CORTX VA, follow the below mentioned procedures, in the order of listing.

- Shutdown the VA

- Start the VA

Shutdown the VA
----------------
1. Stop all S3 I/O traffic from S3 clients to VA.

2. Login to the CORTX Virtual Appliance as user **cortx** and run the following.

 - **sudo su -**

3. Stop CORTX I/O subsystem by running the following command.

 - **hctl shutdown** 

4. After executing the previous command, shutdown the VA by running the following command.

 - **poweroff**
 
Starting the VA
----------------
1. Power on the Virtual Appliance VM.

2. Login to the VA through ssh after the VM starts.

3. As **root**, run the following command.

 - **sudo su -**

4. Start CORTX I/O subsystem by running the following command.

 - **hctl bootstrap -c /var/lib/hare/cluster.yaml**

5. Run the below mentioned command to verify that CORTX I/O subsystem has started.

 - **hctl status** 

6. Run the below mentioned commands to check if CORTX Management subsystem (CSM) has started

 - **systemctl status csm_agent**

 - **systemctl status csm_web**

  If the above services are not active, run the following command.

  - **systemctl start <csm_agent|csm_web>**
