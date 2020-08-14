# Prerequisites
Also check [these pre-prerequisites](Prerequisites) before starting the list below  
  * [x] Check and ensure that the IPMI is configured and BMC IPs are assigned on both nodes.
  * [x] Ensure that **RHEL 7.7 OS is installed** and the kernel version is **3.10.0-1062.el7.x86_64**  
    Use: `lsb_release -a` command.
  * [x] Ensure that **direct network connection is done** between 2 servers nodes for private data network.   
  * [x] Ensure *(XXX How?)* that **SAS connections** from controllers to servers are **NOT cross connected**.
  * [x] Ensure that pools and volumes are created on controller and are mapped to both the servers.
    This can be done via controller web-interface *(XXX Link?)* on "Mappings" page.  

**Run Cortx prerequisite script:**  
  For Provisioner to deploy successfully on RHEL servers the subscription manager either needs to be enabled (with standard RHEL license with RHEL HA license enabled) or completely disabled.  
  Run prerequisite script based on your licenses enabled/disabled on your systems  

  Following section is for RedHat systems, for CentOS directly run the prerequisite script.  

  * **Check whether licenses are enabled (check run on both servers):**  
    - **Check Licenses:**  
      Run following command to check if subscription manager is enabled  
      ```
      $ subscription-manager list | grep Status: | awk '{ print $2 }' && subscription-manager status | grep "Overall Status:" | awk '{ print $3 }'
      Subscribed  
      Current
      $  
      ```     
      1. If above output is seen then subscription manager is enabled. Check if HA license now in next step.    
      2. If not, then subscription manager is disabled. Run prerequisite script mentioned in the "subscription manager disabled" section  

    - **Check if HA license is also enabled:**   
      Run following command to check if High availability license is also enabled:  
      ```     
      # subscription-manager repos --list | grep rhel-ha-for-rhel-7-server-rpms
       Repo ID:   rhel-ha-for-rhel-7-server-rpms
      #  
      ```
      If the ha repo listed as shown above then HA license is also enabled.  
      **If HA repo is not listed** then HA license is not enabled, there are two choices now:  
        1. Get the HA license enabled from Infra team  on both the nodes.  
        2. Deploy with subscription manager disabled on both nodes.   

  * **Run Prerequisite script:**  
    By now it must be clear whether to deploy Cortex with subscription manager disabled or enabled.  
    Based on your findings choose which prerequisite script is to be run:  

    - **subscription manager enabled**:   
   
        ****NOTE :** You need to Generate your own Token for Dev, Release & Beta Build..**  
        **Steps :**  
                  **Goto**: [GitHub](https://github.com/Seagate/cortx-prvsnr)  
                  **Choose**: The popup option **DEV** OR **BETA**  OR go to *release* branch for latest released code
                  **Click**: cli ----> src  
                  **Click**: cortx-prereqs.sh and click **RAW** it will take you to new page, copy the token link for your Prerequisite script.    

      \# `curl https://raw.githubusercontent.com/Seagate/cortx-prvsnr/master/cli/src/cortx-prereqs.sh?token=APVYY2KMSGG3FJBCA73EUZC7B3BYG | bash -s`   
      **NOTE:**: For Beta build deployment, replace `master` with `Cortx-v1.0.0_Beta` in url above.   

    - **subscription manager disabled**:   
      \# `curl https://raw.githubusercontent.com/Seagate/cortx-prvsnr/Cortx-v1.0.0_Beta/cli/src/cortx-prereqs.sh?token=APVYY2OPAHDGRLHXTBK5KIC7B3DYG -o cortx-prereqs.sh; chmod a+x cortx-prereqs.sh; ./cortx-prereqs.sh --disable-sub-mgr`   
      **NOTE:**: For Beta build deployment, replace `master` with `Cortx-v1.0.0_Beta` in url above.  

**NOTE:**  
  1. This will reboot the system only if the Mellanox Drivers are installed, if Mellanox Drivers are already installed no reboot is done.  
  1. This will setup the required RedHat repos- through subscription.  
  1. Seagate internal repos are set up manually in /etc/yum.repos.d/  

  * [x] Ensure mellanox drivers are installed on both nodes using prerequisite script mentioned above     

# Known issues
Before running the commands, please take a look at the known issues:
* [Known Issues for dual node setup](https://github.com/Seagate/cortx-prvsnr/wiki/deploy-eos)

# Dual Node Setup on Hardware
1. Ensure that the [Prerequisites](QuickStart-Guide#prerequisites) are met and both systems are rebooted.  

1.  Ensure that all the volumes/LUNs mapped from storage enclosure to the servers are visible **on both servers**.  
    ```
    [root@sm10-r20 ~]# lsblk -S|grep SEAGATE
    sda  0:0:0:1    disk SEAGATE  5565             G265 sas
    sdb  0:0:0:2    disk SEAGATE  5565             G265 sas
    sdc  0:0:0:3    disk SEAGATE  5565             G265 sas
    sdd  0:0:0:4    disk SEAGATE  5565             G265 sas
    sde  0:0:0:5    disk SEAGATE  5565             G265 sas
    sdf  0:0:0:6    disk SEAGATE  5565             G265 sas
    sdg  0:0:0:7    disk SEAGATE  5565             G265 sas
    sdh  0:0:0:8    disk SEAGATE  5565             G265 sas
    sdi  0:0:1:1    disk SEAGATE  5565             G265 sas
    sdj  0:0:1:2    disk SEAGATE  5565             G265 sas
    sdk  0:0:1:3    disk SEAGATE  5565             G265 sas
    sdl  0:0:1:4    disk SEAGATE  5565             G265 sas
    sdm  0:0:1:5    disk SEAGATE  5565             G265 sas
    sdn  0:0:1:6    disk SEAGATE  5565             G265 sas
    sdo  0:0:1:7    disk SEAGATE  5565             G265 sas
    sdp  0:0:1:8    disk SEAGATE  5565             G265 sas
    [root@sm10-r20 ~]# 
    ```
    **NOTE: If the disk devices aren't listed as shown above please don't proceed, it will fail the deploy.**  
    **Try rebooting the servers and check again, if the disks aren't listed even after reboot, contact Provisioning or Infra team**. 

1. **Deploy Cortx**  
   - **Auto deploy** Cortx in single command **(Recommended)**:  
     Follow [this guide for auto deploy Cortx](https://github.com/Seagate/cortx-prvsnr/wiki/Deployment-on-VM_Auto-Deploy) using a single command  

     **OR**  

   - Deploy Cortx manually **(manual)**:  
     Follow [This guide to Deploy Cortx manually](https://github.com/Seagate/cortx-prvsnr/wiki/Cortx-setup-on-VM-singlenode) using old steps 

# Teardown EOS  
  Follow [this guide](https://github.com/Seagate/cortx-prvsnr/wiki/Teardown-Guide) to teardown EOS components.  

# Virtual Machines
  Follow [this guide to setup Cortx on dual node VM](Cortx-setup-on-VM)
  Follow [this guide to setup Cortx on single node VM](Cortx-setup-on-VM-singlenode)

# S3client setup  
  Follow [this guide](Client-Setup) for s3client setup  

# Single Node Setup on HW
The single node setup is not support till further notice.
The guide can however be referred [here](Single-node-setup)


# References
  * CLI [Help](CLI)
