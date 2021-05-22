# Single-Node VM deployment

### Pre-requisite to setup Virtual Machine:

- Please create VM(s) with at least 2 cores (vCPU) and 4GB of RAM.
- Ensure the VM is created with minimum 6 attached disks (raw) per node

  - Data Disks - Min 4 Disks per node (Capacity 10G+) ex: /dev/sdc,/dev/sdd,/dev/sde,/dev/sdf
  - Metadata Disks - Min 2 Disks per node (Capacity - 10% of total Data Disk Size) ex: /dev/sdb

Note: 
  
- Ensure the VM is created with minimum 3 network Interfaces where,
  - Management IP - ens32
  - Public IP - ens33
  - Private IP - ens34

Note: 
