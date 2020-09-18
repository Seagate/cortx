==================
Importing OVA File
==================

Based on the hypervisor you use, refer the sections below to import an OVA file.

******************
VMware ESX Server
******************
1.

2.

3.

4. <<Reference to Deploying VM document>> 

***************
VMware vSphere
***************

1. Log in to the VMware vSphere web client and go to the **VMs** tab. 

2. At the top of the window, click **Actions**. A menu appears. Then, click **Deploy OVF Template**. A window asking through which you can upload the OVA file appears.

3. Use the **Browse** button to select the OVA file from your system. Click **Next** after the OVA file is selected.

4. Enter a name for your VM and select the location where you want to deploy, then click **Next**.

5. Select the resource that you want to use to run the virtual appliance. Then, click **Next**.

6. Review the details and click **Next**.

7. Select the desired storage location from the list of data stores. Then, click **Next**.

 - You can either choose **Thick** or **Thin**. It is recommended to select **Thick**.

8. Select a network from the drop-down list for each interface, click **Next**.

 - One interface must be mapped to the management network

 - One interface must be mapped to the private data network

 - One interface must be mapped to the public data network

9. Click **Finish** after you review the configuration. The process of importing starts. After the import is complete, click **Refresh**.

10. Navigate to `Deploying a VM <Deploying_a_VM.rst>`_.

11. Perform the following steps:

 - Step 2

 - Step 3

 - Step 4

 - Step 5 

******************
VMware Workstation
******************
Note: With "Bridged" network configuration VMware Workstation should allow DHCP for the installed VMs.   
Some host network adapters might need to be unselected to allow proper operation. See https://stackoverflow.com/questions/31531235/guest-vm-cant-get-ip-address-with-bridge-mode for more details

1. Open the downloaded VMware Workstation Player.

2. Select **Open a Virtual Machine**. The **Open Virtual Machine** window opens.

3. Select the downloaded OVA file, and click **Open**. The **Import Virtual Machine** window is displayed.

4. Enter a name of your choice for the VM, and provide the location where the VM would be stored in the **Storage Path**.

5. Click **Import**. The process of importing the VM gets started.

6. After the importing is complete, navigate to `Deploying a VM <Deploying_a_VM.rst>`_.

7. Perform the following steps:

 - Step 2
 
 - Step 3
 
 - Step 4
 
 - Step 5

**************
VMware Fusion
**************
1. Launch VMware Fusion in your system.

2. Click **File** at the top. A menu appears. Select **Import**

3. Click **Choose File**. From your system, select the OVA file. Then, click **Open**.

4. Enter the name for the virtual machine in the **Save As** text box and provide the location to save it.

 - By default, Fusion creates the Virtual Machines folder.

5. Click **Save**. Fusion performs OVA specification conformance and virtual hardware compliance checks. After the import is complete, the virtual machine appears in the virtual machine library and in a separate virtual machine window.

6. Navigate to `Deploying a VM <Deploying_a_VM.rst>`_.

7. Perform the following steps:

 - Step 2
 
 - Step 3
 
 - Step 4
 
 - Step 5 
