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
Deployment Procedure
**********
Perform the procedure mentioned below to deploy the VM.

1. Import the OVA file by referring to  `Importing OVA <https://github.com/Seagate/cortx/edit/VenkyOS-patch-9/doc/Deploying_a_VM.rst>`_.

2. After the completion of import, open the VM console, and login as **cortx**. The password must be **opensource!**.

3. Become the **root** user by running the following command.

 - sudo su -

4. Run **ip a l** and note the IP addresses of the following interfaces:

  - ens192 - management
  - ens256 - public data
    
5. If required, change the hostname by running the following command:

  - **hostnamectl set-hostname --static --transient --pretty <new-name>**
  
    To verify the change in hostname, run the following command:
    
    - **hostnamectl status**
 
 
**Note**: If you are running the VM in any of the products of VMware, it is not recommended to use **VMware Tools**, as CORTX may break due to kernel dependencies. 

**********
Onboarding Procedure
**********
Point your browser to https://<ens192 IP>:28100/#/preboarding/welcome and follow the procedure to accept EULA and create the initial admin account 

