===========================
Conversion from OVA to OVF
===========================

Follow the below mentioned procedure to convert an OVA file to an OVF file.

#. Download the VMware OVF tool by clicking `here <https://my.vmware.com/group/vmware/get-download?downloadGroup=OVFTOOL430UPDATE3>`_ and install it.

#. After the installation is complete, navigate to the directory where you installed the OVF tool using the CD command. For example, on a windows machine, by default, the location to the directory is as follows.

   ::
   
    C:\Program Files\VMware\VMware OVF Tool
    
#. To convert the OVA to the OVF, run the below mentioned command.

   ::
   
    ovftool.exe *“Path  of the OVA source file” “Path for the destination OVF to be created”*

   In the below example, “demo-vm.ova” is being converted to OVF file “demo-vm.ovf”. We have to specify the full path of the source OVA file and the full path for the destination OVF file.
   
   .. image:: images/OVAOVF.PNG

