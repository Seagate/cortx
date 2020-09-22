==================
Importing the CORTX Open Virtual Appliance [OVA]
==================

Based on the hypervisor you use, click to expand the corresponding section below.

.. raw:: html

    <details>
   <summary><a>VMware vSphere</a></summary>

#. Log in to the VMware vSphere web client and go to the **VMs** tab. 

#. At the top of the window, click **Actions**. A menu appears. Then, click **Deploy OVF Template**. A window asking through which you can upload the OVA file appears.

#. Use the **Browse** button to select the OVA file from your system. Click **Next** after the OVA file is selected.

   .. image:: images/vSphere50.PNG

#. Enter a name for your VM and select the location where you want to deploy, then click **Next**.

   .. image:: images/vSphere77.PNG

#. Select the resource that you want to use to run the virtual appliance. Then, click **Next**.

   .. image:: images/vSphere13.PNG

#. Review the details and click **Next**.

   .. image:: images/vSphere12.PNG

#. Select the desired storage location from the list of data stores. Then, click **Next**.

   - You can either choose **Thick** or **Thin**.
   
 
   .. image:: images/vSphere100.PNG

#. Select a network from the drop-down list for each interface, click **Next**.

   - One interface must be mapped to the management network

   - One interface must be mapped to the private data network

   - One interface must be mapped to the public data network
   
 
   .. image:: images/vSphere150.PNG

#. Click **Finish** after you review the configuration. The process of importing starts. After the import is complete, click **Refresh**.

#. Return to `CORTX on OVA <CORTX_on_Open_Virtual_Appliance.rst>`_, and resume following the instructions here.

  
.. raw:: html
   
   </details>


.. raw:: html


    <details>
   <summary><a>VMware Workstation</a></summary>

#. Open the downloaded VMware Workstation Player.

#. Select **Open a Virtual Machine**. The **Open Virtual Machine** window opens.  

   .. image:: images/WS1.PNG

#. Select the downloaded OVA file, and click **Open**. The **Import Virtual Machine** window is displayed.

   .. image:: images/WS2.PNG

#. Enter a name of your choice for the VM, and provide the location where the VM would be stored in the **Storage Path**. Click **Import**. The process of importing the VM gets started.

   .. image:: images/WS3.PNG

#. After importing the file, power on the VM by clicking on the green arrow or the green text that says, "Power on this virtual machine".

   .. image:: images/power_on_vmw.png

#. After finishing the import, return to `CORTX on OVA <CORTX_on_Open_Virtual_Appliance.rst>`_, and resume following the instructions there.

   **Note**: If you do not see any IP addresses when you run the **ip a l** command as directed in the instructions, you may need to reconfigure some of your virtual networking settings.  Refer to `these instructions <troubleshooting_virtual_networks.rst>`_ for more details.

.. raw:: html
   
   </details>
   

.. raw:: html


    <details>
   <summary><a>VMware ESX Server</a></summary>

#. Login to the VMware ESX server using vSphere client. 

#. At the top, click **File**. A menu is displayed.

#. Select **Deploy OVF Template...**. The **Deploy OVF Template** window is displayed. 

#. Navigate to the location where the OVA file is placed in you system. Select the file and click **Next**. A window displaying the details appear.

#. Click **Next** after verifying the details.

#. Enter a name for your VM and click **Next**.

#. Select the desired storage location from the available data stores using the following radio buttons.

   - **Thick Provision**
 
   - **Thin Provision**
 
#. Select a network from the drop-down list for each interface, and click **Next**.

   - One interface must be mapped to the management network

   - One interface must be mapped to the private data network

   - One interface must be mapped to the public data network
 
#.  Click **Finish** after reviewing your settings.
 
#. Return to `CORTX on OVA <CORTX_on_Open_Virtual_Appliance.rst>`_, and resume following the instructions there.

.. raw:: html
   
   </details>


.. raw:: html

    <details>
   <summary><a>VMware Fusion</a></summary>

#. Launch VMware Fusion in your system.

#. Click **File** at the top. A menu appears. Select **Import**

#. Click **Choose File**. From your system, select the OVA file. Then, click **Open**.

#. Enter the name for the virtual machine in the **Save As** text box and provide the location to save it.

   - By default, Fusion creates the Virtual Machines folder.

#. Click **Save**. Fusion performs OVA specification conformance and virtual hardware compliance checks. After the import is complete, the virtual machine appears in the virtual machine library and in a separate virtual machine window.

#. Return to `CORTX on OVA <CORTX_on_Open_Virtual_Appliance.rst>`_, and resume following the instructions there.

.. raw:: html
   
   </details>


