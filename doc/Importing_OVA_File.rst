==================
Importing OVA File
==================

Based on the hypervisor you use, refer the sections below to import an OVA file.

******************
VMware ESX Server
******************

1. Login to the VMware ESX server using vSphere client. 

2. At the top, click **File**. A menu is displayed.

3. Select **Deploy OVF Template...**. The **Deploy OVF Template** window is displayed. 

4. Navigate to the location where the OVA file is placed in you system. Select the file and click **Next**. A window displaying the details appear.

5. Click **Next** after verifying the details.

6. Enter a name for your VM and click **Next**.

7. Select the desired storage location from the available data stores using the following radio buttons.

 - **Thick Provision**
 
 - **Thin Provision**
 
8. Select a network from the drop-down list for each interface, and click **Next**.

  - One interface must be mapped to the management network

  - One interface must be mapped to the private data network

  - One interface must be mapped to the public data network
 
 9. Click **Finish** after reviewing your settings.
 
 10. Navigate to `CORTX on OVA <CORTX_on_Open_Virtual_Appliance.rst>`_, and follow the instructions from step 3.

***************
VMware vSphere
***************

1. Log in to the VMware vSphere web client and go to the **VMs** tab. 

2. At the top of the window, click **Actions**. A menu appears. Then, click **Deploy OVF Template**. A window asking through which you can upload the OVA file appears.

3. Use the **Browse** button to select the OVA file from your system. Click **Next** after the OVA file is selected.

   .. image:: images/vSphere2.PNG

4. Enter a name for your VM and select the location where you want to deploy, then click **Next**.

   .. image:: images/vSphere3.png

5. Select the resource that you want to use to run the virtual appliance. Then, click **Next**.

   .. image:: images/vSphere4.png

6. Review the details and click **Next**.

7. Select the desired storage location from the list of data stores. Then, click **Next**.

 - You can either choose **Thick** or **Thin**.
 
  .. image:: images/vSphere5.PNG

8. Select a network from the drop-down list for each interface, click **Next**.

 - One interface must be mapped to the management network

 - One interface must be mapped to the private data network

 - One interface must be mapped to the public data network
 
 .. image:: images/vSphere8.PNG

9. Click **Finish** after you review the configuration. The process of importing starts. After the import is complete, click **Refresh**.

  .. image:: images/vSphere7.png

10. Navigate to `CORTX on OVA <CORTX_on_Open_Virtual_Appliance.rst>`_, and follow the instructions from step 3.

******************
VMware Workstation
******************
Note: With "Bridged" network configuration VMware Workstation should allow DHCP for the installed VMs.   
Some host network adapters might need to be unselected to allow proper operation. See https://stackoverflow.com/questions/31531235/guest-vm-cant-get-ip-address-with-bridge-mode for more details

1. Open the downloaded VMware Workstation Player.

2. Select **Open a Virtual Machine**. The **Open Virtual Machine** window opens.

  .. image:: images/WS1.PNG

3. Select the downloaded OVA file, and click **Open**. The **Import Virtual Machine** window is displayed.

4. Enter a name of your choice for the VM, and provide the location where the VM would be stored in the **Storage Path**.

5. Click **Import**. The process of importing the VM gets started.

6. After the importing is complete, navigate to `CORTX on OVA <CORTX_on_Open_Virtual_Appliance.rst>`_, and follow the instructions from step 3.

**************
VMware Fusion
**************

1. Launch VMware Fusion in your system.

2. Click **File** at the top. A menu appears. Select **Import**

3. Click **Choose File**. From your system, select the OVA file. Then, click **Open**.

4. Enter the name for the virtual machine in the **Save As** text box and provide the location to save it.

 - By default, Fusion creates the Virtual Machines folder.

5. Click **Save**. Fusion performs OVA specification conformance and virtual hardware compliance checks. After the import is complete, the virtual machine appears in the virtual machine library and in a separate virtual machine window.

6. Navigate to `CORTX on OVA <CORTX_on_Open_Virtual_Appliance.rst>`_, and follow the instructions from step 3.

