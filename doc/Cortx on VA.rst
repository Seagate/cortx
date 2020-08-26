==========================
Cortx on Virtual Appliance
==========================
Virtual Appliance (VA) is a virtual machine image file that consists of pre-installed and pre-configured operating system environment, and a single application.

This document provides information on the procedure that must be followed to install Cortx (all components included) on a VA.

**************
Prerequisites
**************
The prerequisites required to install Cortx on a VA is listed below.

- GitHub Access
- Unzipping Software (preferably WinZip)
- Either of the following:
 - VMware ESX server
 - VMware ESXi server
 - VMware Workstation
 - VMware Fusion

**********
Procedure
**********
The procedure to install Cortx on VA is mentioned below.

1. Login to `GitHub <https://github.com/>`_ with your credentials.
2. Select the relevant repository, and navigate to the appropriate directory. *<<we need to give links of the repository and directory>>*

3. From the directory, download the ZIP file that contains the VMware virtual machine images.

4. Extract the contents of the downloaded ZIP file into your system.

5. Install the VM on your system by referring *<<Link to the VM Installation document>>*.

 - In case of troubleshooting, refer `VM Documents <https://docs.vmware.com/en/VMware-vSphere/index.html>`_.
 
6. Switch on the virtual machine and login using either the SSH or VMware console.

 - Username: root
 - Password: opensource!


7. Run the following command:

 - **hctl bootstrap --mkfs /var/lib/hare/cluster.yaml**

  You must run the above command with **--mkfs** only once. Further usage of **--mkfs** erases data.

8. Ensure that the I/O stack is running by running the following command:

 - **hctl status**

9. Ensure that the CSM service is operational by running the following commands:

 - **systemctl status csm_agent**
 - **systemctl status csm_web**

   If the above services are not active, run the following command:

  - **systemctl start <csm_agent|csm_web>**
