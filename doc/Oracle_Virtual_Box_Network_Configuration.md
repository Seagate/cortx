# Oracle VirtualBox Network Configuration

This document provides information to configure the network settings on the Oracle VirtualBox. To configure the networks, you need You need to change the Network Device Name from enp0s3, enp0s8, enp0s9 to ens32, ens33 and ens34. The network device name cannot be exact same as listed above, for example, enp0s8, enp0s9, enp0s17 instead of enp0s3, enp0s8, enp0s9.

This setup is using DHCP, the IP changes are bound to happen during restart. The static IP configuration could be harder to maintain as it may not work for different VM with different network setups. 

**Procedure**

1. Run the following command to get your Network Device MAC address (Shown after **link/ether**)
   ```
   ip a l
   ```

2. Record the MAC addresses and go to the following directory:
   ```
   cd /etc/sysconfig/network-scripts/
   vi ifcfg-ens32
   Add a new line under **BOOTPROTO=dhcp
   Add a new parameter with the MAC Address *HWADDR=<MAC-Address>
   Repeat the steps for enp0s8 and enp0s9 respectively
   vi ifcfg-ens33
   vi ifcfg-ens34
   ```

   The sample output **cat ifcfg-ens34**:
      
    ```      
    DEVICE="ens34"
    USERCTL="no"
    TYPE="Ethernet"
    BOOTPROTO="dhcp"
    HWADDR=08:00:27:25:65:74
    ONBOOT="yes"
    PREFIX="24"
    PREDNS="no"
    DEFROUTE="no"
    NM_CONTROLLED="no"
    ZONE=trusted
    ```
   
3. Restart your VM. 

4. To verify the change in Network Device Name, run:

    ``ip a l``

5. To verify the Date/Time is correct, run:

      ``date``

    - If the time displayed is incorrect, use the following command to adjust time for timezone as necessary. If the time is not correctly configure, you might face SSL certificate issue later.

      ``date --set "[+/-]xhours [+/-]yminutes"``
      
      For instance if your timezone is `4:30:00` ahead of UTC, then run the following command in VM. Note the `-` before minutes as well. Similarly if your timezone is behind of UTC, use +ve hours and +ve minutes to make the adjustment.

      ``date --set "-4hours -30minutes"``