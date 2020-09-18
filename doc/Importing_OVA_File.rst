==================
Importing OVA File
==================

Based on the hypervisor you use, refer the sections below to import an OVA file.

******************
VMware ESX Server
******************
#. step 1
#. step 2
#. step 3
#. <<Reference to Deploying VM document>> 

***************
VMware vSphere
***************

#. Log in to the VMware vSphere web client and go to the **VMs** tab. 
#. At the top of the window, click **Actions**. A menu appears. Then, click **Deploy OVF Template**. A window asking through which you can upload the OVA file appears.
#. Use the **Browse** button to select the OVA file from your system. Click **Next** after the OVA file is selected.
#. Enter a name for your VM and select the location where you want to deploy, then click **Next**.
#. Select the resource that you want to use to run the virtual appliance. Then, click **Next**.
#. Review the details and click **Next**.
#. Select the desired storage location from the list of data stores. Then, click **Next**.
   
   * You can either choose **Thick** or **Thin**. It is recommended to select **Thick**.
#. Select a network from the drop-down list for each interface, click **Next**.
   
   * One interface must be mapped to the management network
   * One interface must be mapped to the private data network
   * One interface must be mapped to the public data network
#. Click **Finish** after you review the configuration. The process of importing starts. After the import is complete, click **Refresh**.
#. Navigate to `Deploying a VM <Deploying_a_VM.rst>`_.
#. Perform the following steps: 
   
   * Step a
   * Step b
   * Step c
   * Step d 

******************
VMware Workstation
******************
Note: With "Bridged" network configuration VMware Workstation should allow DHCP for the installed VMs.   
Some host network adapters might need to be unselected to allow proper operation. See https://stackoverflow.com/questions/31531235/guest-vm-cant-get-ip-address-with-bridge-mode for more details

#. Open the downloaded VMware Workstation Player.
#. Select **Open a Virtual Machine**. The **Open Virtual Machine** window opens.
#. Select the downloaded OVA file, and click **Open**. The **Import Virtual Machine** window is displayed.
#. Enter a name of your choice for the VM, and provide the location where the VM would be stored in the **Storage Path**.
#. Click **Import**. The process of importing the VM gets started.
#. After the importing is complete, navigate to `Deploying a VM <Deploying_a_VM.rst>`_.
#. Perform the following steps:
   
   * step 2
   * step 3
   * step 4
   * step 5

**************
VMware Fusion
**************
#. Launch VMware Fusion in your system.
#. Click **File** at the top. A menu appears. Select **Import**
#. Click **Choose File**. From your system, select the OVA file. Then, click **Open**.
#. Enter the name for the virtual machine in the **Save As** text box and provide the location to save it.
   * By default, Fusion creates the Virtual Machines folder.
#. Click **Save**. Fusion performs OVA specification conformance and virtual hardware compliance checks. After the import is complete, the virtual machine appears in the virtual machine library and in a separate virtual machine window.
#. Navigate to `Deploying a VM <Deploying_a_VM.rst>`_.
#. Perform the following steps:
   
   * step 2
   * step 3
   * step 4
   * step 5
