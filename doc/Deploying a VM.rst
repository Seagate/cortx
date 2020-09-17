================================
Deploying a Virtual Machine (VM)
================================
This document provides information on the procedure to be followed to deploy a VM.

**************
Prerequisites
**************
The prerequisites mentioned below must be met to deploy the VM.

- Pop-ups from ESX server must be allowed.
- ZIP archive with the VM image file must be downloaded and extracted.
- ESX server must be configured with at least two virtual interfaces.

 - One interface for management network
 - One interface for data network
 - One interface for private data network 
 

**********
Procedure
**********
Perform the procedure mentioned below to deploy the VM.

1. Import the OVA file.

2. Perform the network configuration accurately.

  - One interface must be mapped to the management network.
  
  - One interface must be mapped to the data network.
  
  - One interface must be mapped to the private data network. 
  
  **Note**: Ensure that all the three networks are connected to different subnets.

3. Select either of the following as an option for provisioning the disks.

  - **Thin** 
  - **Thick**

4. Ensure accuracy in the configuration. The VM provisioning process takes approximately 25-40 minutes.

5. After the completion of import, open the VM console, and login as **cortx**.

6. Become the **root** user by running the following command.

 - sudo su -

7. Run **ip a l** and note the IP addresses of the following interfaces:

  - ens192 - management
  - ens256 - public data
    
8. If required, change the hostname by running the following command:

  - **hostnamectl set-hostname --static --transient --pretty <new-name>**
  
    To verify the change in hostname, run the following command:
    
    - **hostnamectl status**
 
 
**Note**: If you are running the VM in any of the products of VMware, it is not recommended to use **VMware Tools**, as CORTX may break due to kernel dependencies. 


