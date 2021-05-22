# Single-Node VM deployment

### Pre-requisite to setup Virtual Machine:

- Please create VM(s) with at least 2 cores (vCPU) and 4GB of RAM.
- Ensure the VM is created with minimum of total 6 attached disks (where 5 should be raw disks) per node

Note: raw disks should be added in your hypervisor once CentOS 7.8 is installed

  - Data Disks - Min 4 Disks per node (Capacity 10G+)
  - Metadata Disks - Min 2 Disks per node (Capacity - 10% of total Data Disk Size)
  
- Ensure the VM is created with minimum 3 network Interfaces
