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
 

**********
Procedure
**********
Perform the procedure mentioned below to deploy the VM.

1. Login to the web interface of ESX.

2. Click **Create/Register VM**. The **New Virtual Machine wizard** opens.

3. On the Select creation type page of the wizard, select **Deploy a virtual machine from an OVF or OVA** file, and click **Next**.

4. Enter a name for your virtual machine. Virtual machine names can contain up to 80 characters and must be unique within each ESXi instance.

5. Click the blue pane to select an OVF and a VMDK, or an OVA file to deploy. Your local system storage opens.

6. Select all the files that corresponds to the virtual machine that you want to deploy, and click **Open**.

7. Click **Next**.

8. Select a datastore from the list of accessible datastores. By default, it is **datastore1**.

9. Click **Next**. 

10. Perform the network configuration accurately.

  - One interface must be mapped to the management network.
  
  - One interface must be mapped to the data network.
  
  **Note**: Ensure that the management and data networks are connected to different subnets.

11. Select either of the following as an option for provisioning the disks.

  - **Thin** 
  - **Thick**

12. After provisioning, select the checkbox if you want to start the VM. By default, **Yes** is the option.

13. Click **Next**.

14. Ensure accuracy in the configuration and click **Finish**. The VM provisioning process takes approximately 25-40 minutes.

15. After the completion of import, open the VM console in ESX, and login as **root**.

16. Run **ip a l** and note the IP addresses of the following interfaces:

  - ens192 - management
  - ens256 - public data
  
  **Note**: As there is a single-node VM, the above IP addresses are also VIPs.
  
17. If required, change the hostname by running the following command:

  - **hostnamectl set-hostname <new-name>**
  
    To verify the change in hostname, run the following command:
    
    - **hostnamectl status**
 
 
**Note**: It is not recommended to install **VMware Tools** as CORTX may break due to kernel dependencies. 


