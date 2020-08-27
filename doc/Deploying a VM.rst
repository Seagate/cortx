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
 - one interface for data network
 

**********
Procedure
**********
Perform the procedure mentioned below to deploy the VM.

1. Login to the web interface of ESX.

2. Click **Create/Register VM**. The **New Virtual Machine wizard** opens.

3. On the Select creation type page of the wizard, select **Deploy a virtual machine from an OVF or OVA** file, and click **Next**.

4. Enter a name for your virtual machine. Virtual machine names can contain up to 80 characters and must be unique within each ESXi instance.

5. Click the blue pane to select an OVF and a VMDK, or an OVA file to deploy. Your local system storage opens.

6. Select the file that you want to deploy your virtual machine from and click **Open**. The file you selected is displayed in the blue pane.

7. Click **Next**.

8. Click a datastore from the list of accessible datastores on the Select storage page of the **New Virtual Machine** wizard. By default, it is **datastore1**.

9. Click **Next**.

10. Perform the network configuration accurately.

11. Select the provisioning for the disks option from **Thin** or **Thick**.

12. After provisioning, select the checkbox if you want to start the VM. By default, **Yes** is the option.


**Note**: It is not recommended to use VMware products as CORTX may break due to kernel dependencies. 


