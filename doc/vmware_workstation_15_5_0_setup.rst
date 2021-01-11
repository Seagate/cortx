
For WMware worktation 15.5.0
============================

If your installation of VMWare workstation not work with latest version of Windows, you could install old version of WMware workstation.

Here the steps for running `cortx-va-1.0.2.zip </releases/download/VA/cortx-va-1.0.2.zip>`_ for WMware Workstation 15.5.0 in Window 10 version 1890 ( OS build 17763.1637) 

#. Install `WMware Workstation 15.5.0 <https://www.youwindowsworld.com/en/downloads/virtualization/vmware/vmware-workstation-15-pro/download-535-vmware-workstation-15-pro>`_. 
   (Offial VMWare no longer provided old version for download).

#. After import OVA image to VMware Workstation, go to Menu > Edit > Virtual Network Editor...

   Follow instruction `here <https://github.com/Seagate/cortx/blob/main/doc/troubleshoot_virtual_network.rst>`_  for the IP netowrk setup.
   
   If is not working, you need to click on restore default button to restore all network state. 
   
   .. image:: images/vmsetup1550/restoreDefault.jpg
   
   You might see all IP is same after running **ip a l**, follow instruction below to configure your network.



If your IP address for all vm is same
-------------------------------------

Right click image and select settings.

   .. image:: images/vmsetup1550/vmsettings.jpg
   

#. Click on network adapter, select network connection as briged, check the "replicate pyshical network connection state.

   .. image:: images/vmsetup1550/vm1.jpg


#. Select network adapter 2, select Host Only for network connection option.

   .. image:: images/vmsetup1550/vm2.jpg

#. Select network adpater 3, select NAT for network connection option.

   .. image:: images/vmsetup1550/vm3.jpg


#. Click OK, restart VM and run **ip a l**

you should see the ip settings is correct, you could go to Menu > Edit > Virtual Network Editor, verify that the VMs should had difference IP.

   .. image:: images/vmsetup1550/vm3.jpg

Start the CORTX servr, now you should able to view CORTX UI with following magement IP address as highlighted below:

   .. image:: images/vmsetup1550/ui_ip.jpg

Open browser and go to https://<management IP>:28100/#/preboarding/welcome, you should see CORTX UI.





