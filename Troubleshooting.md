We've listed all the known issues with the CORTX software and the steps to resolve them below. Please refer to our [support documentation](SUPPORT.md) if you want to report any new issues or suggest improvements to any of the below.  Or, as always, please feel free to directly edit the below and submit a Pull Request if you see areas for improvement.

# Known Issues

<details>
<summary>Firmware bundle upload and update does not work</summary>
<p>

### Symptoms and causes
  
  1. Failed to fetch update status.
  2. Failed to upload firmware bundle file.
  3. Failed to update the firmware.

### Resolution

  1. Failed to fetch update status.
    a. Refresh CMP UI after 30 secs.
  2. Failed to upload firmware bundle file.
    a. File format is incorrect. Please upload .bin file only.
    b. File size is too large. Max file allowed is 2 GB.
  3. Failed to update the firmware.
    a. Failure reason as per return by provisioner component. Please check the csm_agent and provisioner logs.
    
 </p>
 </details>
 
<details>
<summary>Software package upload and update is not working</summary>
<p>

### Symptoms and causes

1. Failed to fetch update status.
2. Failed to upload software bundle file.
3. Failed to update the software.

### Resolution

1. Failed to fetch update status.
  a. Refresh CMP UI after 30 secs.
2. Failed to upload software bundle file.
  a. File format is incorrect, please upload .iso file only.
  b. File size is too large, maximum file size allowed is 2 GB.
3. Failed to update the software.
  a. Failure reason as per return by provisioner component. Please check the csm_agent and provisioner logs.
  
 </p>
 </details>
  
<details>
<summary>CMP UI is not working</summary>
<p>

### Symptoms and causes

1. URL is incorrect.
2. csm_web service is not running. Run the command:
  
    `pcs resource show | grep csm-web`

### Resolution

1. URL is incorrect. Please enter correct Mgmt_Vip, port, and url.
2. csm_web service is not running.
3. If not in active state please fire the below command:
   
   `pcs resource enable csm-web`
4. If problem is not solved:
   
   `pcs resource cleanup csm-web`
5. Please refer to HA chapter for more information.

 </p>
 </details>
 
 <details>
 <summary>Capacity Dial showing Total Space as 0.0 KB</summary>
 <p>
    
### Symptoms and causes

Storage details in capacity widget is incorrect.

### Resolution

Refresh CMP UI after 30 secs.

</p>
</details>

<details>
  <summary>Login failed on CMP UI</summary>
  <p>
    
### Symptoms and causes

1. Admin user not created using pre-boarding.
2. Wrong username or password entered.
3. csm agent not running.

### Resolution

1. Refresh CMP UI after 30 secs.
2. Check whether CMP admin user is created or not. Fire the below command to check if CMP admin user is created or not:

    ```shell

    /opt/seagate/cortx/hare/bin/consul kv get -recurse
    cortx/base/user_collection/obj
    ```

    **Output**

      ```shell
      eos/base/user_collection/obj/admin:{"user_id": "admin",
      "user_type": "csm", "roles": ["admin", "manage"],
      "password_hash":
      "$2b$12$ZAFTHJyeu01wmQBrkk0AsuckVEeLU0z.EriD8IWBmhcqVrrxfK2kq", 
      "updated_time": "2020-05-25T08:38:56.151772+0000",
      "created_time": "2020-05-25T08:38:56.151757+0000"}
      ```
3. If you do not get the output as shown above, please create an admin user from CMP UI pre-boarding:

     https://<Mgmt VIP>:28100/#/preboarding/welcome
  
4. Please check the username or password entered. 
  1. If you have forgotten your password, navigate to Monitor/Manage User **>** Please contact administrator.
  2. If you are the Admin User, delete the admin user from the consul by following the commands below and redo the pre-boarding steps.
  
      `/opt/seagate/cortx/hare/bin/consul kv delete`
   
      `cortx/base/user_collection/obj/admin`
    
  </p>
  </details>
  
  <details>
  <summary>Alerts not coming on UI</summary>
  <p>
  
  ### Symptoms and causes
  
  ElasticSearch not running.
  
  ### Resolution
  
  1. Check whether Elasticsearch is running using:
  
      `csm_test -f /opt/seagate/cortx/csm/test/test_data/args.yaml -t`
      
      `/opt/seagate/cortx/csm/test/plans/self_test.pln`
  2. If Elasticsearch is not active, please refer to the provisioning chapter for more information 
  
      **ToDo:** Add link to the relevant Provisioner content.

  </p>
  </details>
  
  <details>
  <summary>Support Bundle Generation Fails or not uploaded on FTP</summary>
  <p>
  
  ### Symptoms and causes
  
  1. Support Bundle command is not running with root privileges.
  2. FTP Configured is not accessible.

  ### Resolution
  
  1. Run the following:
    
      `$ sudo csmcli support_bundle generate <comment>`
  2. Support bundle command needs root privileges so needs to be run always with root privileges.
  3. If the FTP Upload fails, check the FTP Config for Support Bundle by running:
      
      `$ csmcli support_bundle show_config`
      
  4. If FTP Upload fails check the following:
      1. User and Password configured are correct.
      2. Host and Port is reachable through machine.
      3. User has permissions to upload the files on supplied remote path.
      
 </p>
  </details>
      
<details>
  <summary>Unable to view/download Audit logs.</summary>
  </p>
  
  ### Symptoms and causes
  
1. Rsyslog not configured correctly.
2. Rsyslog is not running.

### Resolution

1. Please check whether rsyslog host and port are correctly configured in csm.conf using:

    `Csm.conf path : /etc/csm/csm.conf`
2. Check rsyslog is running using:
    
    `Systemctl status rsyslog`
2. If rsyslog is not active, run:
    
    `Systemctl start rsyslog`
    
</p>
  </details>
  
  <details>
  <summary>After uploading SSL Certificate, CMP SSL Page stops responding</summary>
  <p>

### Symptoms and causes

Provisioner API Fails for fetching SSL Certificates.

### Resolution

There are some known scenarios where cluster goes into maintenance state and doesn't recovers from it. This happens when STONITH resources didn't came back online. To recover the cluster, it has to be done manually.

#### Manual Recovery steps:

1. Use the following command to get back system to normal mode.
    
    `$csmcli system startup`
2. Then check pcs status.
3. If haproxy resource is in failed state then the certificate entered is invalid.

#### Steps to recover:
1. Remove 

    `/opt/seagate/cortx-prvsnr/srv_user/components/misc_pkgs/ssl_certs/files/stx.pem`
2. Run
    
    `$ salt "*" state.apply components.misc_pkgs.ssl_certs`

3. Restart the cluster using:
    
    [pcs cluster stop --all & pcs cluster start --all]
    
</p>
</details>

<details>
  <summary>Health Page on CMP is Blank or has no Data</summary>
  <p>
    
### Symptoms and causes

Schema File for Health map is not Present or File doesn’t have enough permissions

   `File /opt/seagate/cortxprvsnr/generated_configs/healthmap/ees-schema.json`

### Solution

Provide permissions to file
    
   `$ chmod 777 /opt/seagate/cortxprvsnr/generated_configs/healthmap/ees-schema.json`
   
</p>
</details>

<details>
  <summary>CLI Help Section Shows Only Support Bundle Command in Help after Login</summary>
  <p>
    
### Symptoms and causes

CMP Agent is Down.

### Resolution

1. Try connecting to another node to check whether the CMP Agent Service is running on the other node. You can check it via the command:
    
    `pcs resource show | grep csm-agent`
2. If the CMP Agent Service is running, please use csmcli on this node.
3. If CMP Backend Service is down on both nodes, you won’t be able to access the cli commands.
4. Restart csm_agent with following commands: 

    `Restart CSM Agent`
    
    `Pcs resource enable csm-agent`
    
 </p>
</details>

<details>
  <summary>Stats are not working or not visible or large offset in graph</summary>

### Symptoms and cause

Some of the resources like the csm-agent, els-search-clone, statsd-clone, and kibana failed

### Resolution

1. Check `$ pcs status` to list the services that are actively running (service: csm-agent, els-search-clone, statsd-clone, kibana)
2. Run the command: 

    `$ pcs resource cleanup <name of the failed resource>`
3. Check time in cluster system and system where browser is running, they can be different timezone but UTC time must be in sync.

</p>
</details>


    
