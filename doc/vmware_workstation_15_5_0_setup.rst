
For WMware worktation 15.5.0
============================

If you using WMware not working with latest version of Windows and you need to install old version of WMware. Thing might work a bit difference.

Hence is the steps for running `cortx-va-1.0.2.zip <https://github.com/Seagate/cortx/releases/download/VA/cortx-va-1.0.2.zip>`_ for WMware Workstation 15.5.0 `cortx-va-1.0.2.zip <https://github.com/Seagate/cortx/releases/download/VA/cortx-va-1.0.2.zip>`_ in Window 10 version 1890 ( OS build 17763.1637) 

1) Install the `WMware Workstation 15.5.0 <https://www.youwindowsworld.com/en/downloads/virtualization/vmware/vmware-workstation-15-pro/download-535-vmware-workstation-15-pro>`_.

2) All the step will same as the `install instructions <https://github.com/Seagate/cortx/blob/main/doc/CORTX_on_Open_Virtual_Appliance.rst>`_  except VM settings.

3) After import OVA image to VMware Workstation, go to Menu > Edit > Virtual Network Editor...

   Follow instruction `here <https://github.com/Seagate/cortx/blob/main/doc/troubleshoot_virtual_network.rst>`_  for the IP netowrk setup.
   
   If is not working, you need to click on restore default button to restore all network state. 
   
   .. image:: images/vmsetup1550/restoreDefault.jpg
   
   You might see all IP is same after running **ip a l**, follow instruction below to configure your network.



If your IP address for all vm is same
-------------------------------------

Right click image and select settings.

   .. image:: images/vmsetup1550/vmsettings.jpg
   

1) Click on network adapter, select network connection as briged, check the "replicate pyshical network connection state.

   .. image:: images/vmsetup1550/vm.jpg


2) Select network adapter 2, select Host Only for network connection option.

   .. image:: images/vmsetup1550/vm2.jpg

2) Select network adpater 3, select NAT for network connection option.

   .. image:: images/vmsetup1550/vm2.jpg


7) Click OK, restart VM and run **ip a l**

you should see the ip settings is correct, you could go to Menu > Edit > Virtual Network Editor, verify that the VMs should had difference IP.

   .. image:: images/vmsetup1550/vm2.jpg

Start the CORTX servr, now you should able to view CORTX UI with following IP address:

   .. image:: images/vmsetup1550/ui_ip.jpg

Open browser and go to https://192.168.80.128:28100/, you should see CORTX UI as below:

   .. image:: images/vmsetup1550/ui.JPG






