
===============================
CORTX on Open Virtual Appliance
===============================
Open Virtual Appliance (OVA) is a virtual machine (VM) image file that consists of pre-installed and pre-configured operating system environment, and a single application.

This document describes how to use a VM image pre-packaged with CORTX for the purposes of single-node CORTX testing.

***********************
Recommended Hypervisors
***********************
All of the following hypervisors should work:

* VMware ESX server
* VMware vSphere
* VMware Workstation
* VMware Fusion

**********
Procedure
**********
The procedure to install CORTX on OVA is mentioned below.

1. From `our release page <https://github.com/Seagate/cortx/releases/tag/OVA>`_, download the cortxva-v1.1.zip file that contains the virtual machine images.

2. Extract the contents of the downloaded file into your system. You can also run the below mentioned command to extract the content.

   * **gzip cortxva-v1.1.zip**

3. Import the OVA file by referring to `Importing OVA <Importing_OVA_File.rst>`_.

   - In case of troubleshooting, refer to `VM Documents <https://docs.vmware.com/en/VMware-vSphere/index.html>`_.
  
**Important**: If you are running the VM in any of the products of VMware, it is not recommended to use VMware Tools, as CORTX may break due to kernel dependencies.
 
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


8. Set the **Network Translation Address** (NAT) in the hypervisor settings for the imported OVA. Refer to `Importing OVA <Importing_OVA_File.rst>`_.

9. Bring up the OVA by running the below mentioned script.

 - **sh /opt/seagate/cortx/provisioner/cli/virtual_appliance/bootstrap.sh**
 
10. Open the web browser and navigate to the following location:

   * **https://<management IP>:28100/#/preboarding/welcome**
  
**Note**: Operating system updates are not supported due to specific kernel dependencies.

11. Refer to `Onboarding into CORTX <Preaboarding_and_Onboarding.rst>`_ to execute the preboarding and onboarding process.

If you have a firewall between the OVA and the rest of your infrastructure, including but not limited to S3 clients, web browser, and so on, ensure that the  ports mentioned below are open to provide access to OVA.
  
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
 |         28100        |   TCP (HTTPS)     |              Management network             |
 +----------------------+-------------------+---------------------------------------------+

Restarting CORTX OVA
====================
To restart the CORTX OVA, follow the below mentioned procedures, in the order of listing.

- Shutdown the OVA

- Start the OVA

Shutdown the VA
----------------
1. Stop all S3 I/O traffic from S3 clients to VA.

2. Login to the CORTX Virtual Appliance as **cortx** and run the following.

   * **sudo su -**

3. Stop CORTX I/O subsystem by running the following command.

   * **hctl shutdown** 

4. After executing the previous command, shutdown the OVA by running the following command.

   * **poweroff**
 

Starting the OVA
-----------------
1. Power on the Virtual Appliance VM.

2. Login to the OVA through ssh after the VM starts.

3. Bring up the OVA by running the below mentioned script.

 - **sh /opt/seagate/cortx/provisioner/cli/virtual_appliance/bootstrap.sh**
