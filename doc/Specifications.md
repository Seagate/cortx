# Single-Node VM deployment

### Pre-requisite to setup Virtual Machine:

- Please create VM(s) with at least 2 cores (vCPU) and 4GB of RAM.
- Ensure the VM is created with minimum 6 attached disks (raw) per node

  - Data Disks - Min 4 Disks per node (Capacity 10G+) ex: /dev/sdc,/dev/sdd,/dev/sde,/dev/sdf
  - Metadata Disks - Min 2 Disks per node (Capacity - 10% of total Data Disk Size) ex: /dev/sdb

Note: You can find the devices on node using the below command to update in config.ini section,
    
    ```device_list=$(lsblk -nd -o NAME -e 11|grep -v sda|sed 's|sd|/dev/sd|g'|paste -s -d, -)```

  - Values for storage.cvg.0.metadata_devices:
    echo ${device_list%%,*}

  - Values for storage.cvg.0.data_devices:
    echo ${device_list#*,}
  
- Ensure the VM is created with minimum 3 network Interfaces where,
  - Management IP - ens32
  - Public IP - ens33
  - Private IP - ens34

Note: You can find the interfaces as per your setup by running,

    ```firewall-cmd --get-active-zones```
