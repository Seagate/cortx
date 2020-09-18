==========================
CORTX on Virtual Appliance
==========================
Virtual Appliance (VA) is a virtual machine image file that consists of pre-installed and pre-configured operating system environment, and a single application.

This document provides information on the procedure that must be followed to install CORTX (all components included) on a VA.

**************
Prerequisites
**************
The prerequisites required to install CORTX on a VA is listed below.

- Any of the following:
 - VMware ESX server
 - VMware vSphere
 - VMware Workstation
 - VMware Fusion

**********
Procedure
**********
The procedure to install CORTX on VA is mentioned below.

1. From `OVA file <https://github.com/Seagate/cortx/releases/tag/VA>`_, download the TAR file that contains the VMware virtual machine images (a file named **cortxvm_opensource_vX.tar.gz**, where X is the revision of the VM image).

2. Extract the contents of the downloaded TAR file into your system. You can also run the below mentioned command to extract the content.

 - **tar -xzvf cortvm_opensource_vX_ova.tar.gz**

3. Import the OVA file by referring to `Importing OVA <Importing_OVA_File.rst>`_.

 - In case of troubleshooting, refer to `VM Documents <https://docs.vmware.com/en/VMware-vSphere/index.html>`_.
 
4. Open the VM console, and login with the below mentioned credentials.

  - Username: **cortx**
  
  - Password: **opensource!**

5. Become the **root** user by running the following command.

 - sudo su -
 
6. Run **ip a l** and note the IP addresses of the following interfaces:

 - ens192 - management
 
 - ens256 - public data
 
7. Change the hostname by running the following command:

 - **hostnamectl set-hostname --static --transient --pretty <new-name>**
  
     If you receive **Access denied** message, remove immutable settings on the **/etc/hostname** file and run the command again. To remove immutable setting from **/etc/hostname**, run the following command.
     
      - **chattr -i /etc/hostname**
  
 
   To verify the change in hostname, run the following command:
 
   - **hostnamectl status**
   
   **Note**: Both short hostnames and FQDNs are accepted. If you do not have DNS server to register the VM with, you can access it using the IP address. However, the hostname is mandatory and should be configured.
  
8. Update **/etc/hosts** with the management IP address and the new hostname for the VA. In the same file, update the line that contains **s3.seagate.com** with the IP address of the public data interface. Do not remove or rename hostnames in this line.

9. Edit **/root/.ssh/config** and update the following with the new hostname for the VA.

  - **Host srvnode-1 <new_hostname>**
  
  - **HostName <new_hostname>**
  
  **Note**: Please keep **srvnode-1** in the Host field. This is an internal name and it's required for the proper functioning of VA.

10. Refresh HAproxy configuration by running the following command.

  - **salt "*" saltutil.pillar_refresh**
  
  - **salt "*" state.apply components.ha.haproxy.config**
  
  - **salt "*" state.apply components.ha.haproxy.start**
  
11. Restart lnet by running the following command.

  - **systemctl restart lnet**
  

12. Run the following command:

 - **hctl bootstrap --mkfs /var/lib/hare/cluster.yaml**

  You must run the above command with **--mkfs** only once. Further usage of **--mkfs** erases data.

13. Ensure that the I/O stack is running by running the following command:

 - **hctl status**

14. Ensure that the CSM service is operational by running the following commands:

 - **systemctl status csm_agent**
 - **systemctl status csm_web**

   If the above services are not active, run the following command:

  - **systemctl start <csm_agent|csm_web>**
  
15. Open the web browser and navigate to the following location:

  - **https://<management IP>:28100/#/preboarding/welcome**
  
**Note**: Operating system updates are not supported due to specific kernel dependencies.

16. Refer `Onboarding into CORTX <Onboarding.rst>`_ to execute the onboarding process.


If you have a firewall between the VA and the rest of your infrastructure, including but not limited to S3 clients, web browser, and so on, ensure that the  ports mentioned below are open to provide access to VA.
  
 +----------------------+-------------------+---------------------------------------------+
 |    **Port number**   |   **Protocols**   |   **Destination network (on VA)**           |
 +----------------------+-------------------+---------------------------------------------+
 |          22          |        TCP        |           Management network                |
 +----------------------+-------------------+---------------------------------------------+ 
 |          53          |      TCP/UDP      | Management network and Public Data network  |
 +----------------------+-------------------+---------------------------------------------+ 
 |         123          |      TCP/UDP      |              Management network             |
 +----------------------+-------------------+---------------------------------------------+
 |         443          |       HTTPS       |             Public Data network             |
 +----------------------+-------------------+---------------------------------------------+
 |         9443         |       HTTPS       |              Public Data network            |
 +----------------------+-------------------+---------------------------------------------+
 |         28100        |   TCP (HTTPS)     |              Public Data network            |
 +----------------------+-------------------+---------------------------------------------+


  
Restarting CORTX VA
===================
To restart the CORTX VA, follow the below mentioned procedures, in the order of listing.

- Shutdown the VA

- Start the VA

Shutdown the VA
----------------
1. Stop all S3 I/O traffic from S3 clients to VA.

2. Login to the CORTX Virtual Appliance as **cortx** and run the following.

 - **sudo su -**

3. Stop CORTX I/O subsystem by running the following command.

 - **hctl shutdown** 

4. After executing the previous command, shutdown the VA by running the following command.

 - **poweroff**
 
Starting the VA
----------------
1. Power on the Virtual Appliance VM.

2. Login to the VA through ssh after the VM starts.

3. Login to the CORTX VA as **cortx** and run the following.

 - **sudo su -**

4. Start CORTX I/O subsystem by running the following command.

 - **hctl bootstrap -c /var/lib/hare/**

5. Run the below mentioned command to verify that CORTX I/O subsystem has started.

 - **hctl status** 

6. Run the below mentioned commands to check if CORTX Management subsystem (CSM) has started.

 - **systemctl status csm_agent**

 - **systemctl status csm_web**

  If the above services are not active, run the following command.

  - **systemctl start <csm_agent|csm_web>**
