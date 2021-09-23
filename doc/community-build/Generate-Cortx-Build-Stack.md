# Compile and Build Complete CORTX Stack using Docker

This document provides step-by-step instructions to build and generate the CORTX stack packages using Docker.

To know about various CORTX components, see [CORTX Components guide](https://github.com/Seagate/cortx/blob/main/doc/Components.md).

## Prerequisites

- All the prerequisites specified in the [Building the CORTX Environment for Single Node](Building-CORTX-From-Source-for-SingleNode.md) must be satisfied.
- Run the appropriate tag as per OS required i.e. CentOS 7.8 or CentOS 7.9. For example:

  - For CentOS 7.8.2003:
    ```
    docker pull ghcr.io/seagate/cortx-build:centos-7.8.2003
    ```
  - For CentOS 7.9.2009:
    ```
    docker pull ghcr.io/seagate/cortx-build:centos-7.9.2009
- Do not update OS or kernel package with `yum update` as the kernel version must be set to `3.10.0-1160.el7` 


## Procedure

1. Run the following command to clone the CORTX repository:

   ```
   cd /root && git clone https://github.com/Seagate/cortx --recursive --depth=1
   ```

2. Run the following command to check out the codebase from the **main** branch for all components:

   ```
   docker run --rm -v /root/cortx:/cortx-workspace ghcr.io/seagate/cortx-build:centos-7.9.2009 make checkout BRANCH=main
   ```

3. Run the following command to create a directory to store packages:

   ```
   mkdir -p /var/artifacts/ && mkdir -p /mnt/cortx/{components,dependencies,scripts}
   ```

4. Run the following command to build the CORTX packages:

   ```
   docker run --rm -v /var/artifacts:/var/artifacts -v /root/cortx:/cortx-workspace ghcr.io/seagate/cortx-build:centos-7.9.2009 make clean build
   ```

5. Run the following command to generate the ISO for each component:

   ```
   docker run --rm -v /var/artifacts:/var/artifacts -v /root/cortx:/cortx-workspace ghcr.io/seagate/cortx-build:centos-7.9.2009 make iso_generation
   ```

6. The CORTX build is generated in the directory created at step 3. To view the generated build, run:

   ```
   [root@deploy-test yum.repos.d]# ll /var/artifacts/0/
   total 1701408
   drwxr-xr-x  13 root root        231 Sep 10 15:38 3rd_party
   drwxr-xr-x   3 root root       4096 Sep 10 15:38 cortx_iso
   -rw-r--r--   1 root root      19416 Sep 10 15:38 install-2.0.0-0.sh
   drwxr-xr-x   2 root root         38 Sep 10 15:42 iso
   drwxr-xr-x 198 root root       8192 Sep 10 15:38 python_deps
   -rw-r--r--   1 root root  241635505 Jun 15 20:15 python-deps-1.0.0-0.tar.gz
   -rw-r--r--   1 root root 1500564340 Jul 22 17:46 third-party-1.0.0-0.tar.gz
   ```
 
## Compile and Build CORTX Stack as per Individual component

7. To view each component targets, run:
   ```
   docker run ghcr.io/seagate/cortx-build:centos-7.9.2009 make help
   ```
   
   The system output displays as follows:
   ```
   [root@ssc-vm-1613 cortx-**]# docker run ghcr.io/seagate/cortx-build:centos-7.9.2009 make help
   usage: make "target"
   
   Please clone required component repositories in cortx-workspace directory before executing respective targets.

   targets:

     help: print this help message.

     clean: remove existing /var/artifacts/0 directory.

     build: generate complete CORTX build including third-party-deps at "/var/artifacts/0"

     control-path: generate control-path packages. cortx-provisioner, cortx-monitor, cortx-manager, cortx-management-portal and cortx-ha.

     io-path: generate io-path packages. cortx-motr, cortx-s3server and cortx-hare.

     cortx-motr: generate cortx-motr packages.

     cortx-s3server: generate cortx-s3server packages.

     cortx-hare: generate cortx-hare packages.

     cortx-ha: generate cortx-ha packages.

     cortx-management-portal: generate cortx-management-portal packages.

     cortx-manager: generate cortx-manager packages.

     cortx-monitor: generate cortx-monitor packages.

     cortx-posix: generate cortx-posix (NFS) packages.

     cortx-prvsnr: generate cortx-prvsnr packages.

     iso_generation: generate ISO file from release build.
     ```

8. Deploy the packages generated to create CORTX cluster using the instruction provided in [Deploy Cortx Build Stack guide](ProvisionReleaseBuild.md).

## Troubleshooting

You might get an error message about missing `kernel-devel` package when building the CORTX packages; sample errors:
```sh
Error: No Package found for kernel-devel = 3.10.0-1127.19.1.el7
error: Failed build dependencies:
            kernel-devel = 3.10.0-1127.19.1.el7 is needed by cortx-motr-2.0.0-0_git2ca587c_3.10.0_1127.19.1.el7.x86_64
```
**Here is the solution:**
1. Go inside the `Docker` container using the interactive mode by running:
```sh
docker container run -it --rm -v /var/artifacts:/var/artifacts -v /root/cortx:/cortx-workspace ghcr.io/seagate/cortx-build:centos-7.8.2003 bash
```
2. Check whether you have `kernel-devel` installed by running `rpm -qa | grep kernel-devel`. If you don't have it, please download the required kernel-devel RPM and then install it.
3. If the `kernel-devel` is installed and you still get the error above, the root cause must be due to the version mismatch. Here is the thing, the `Makefile` script inside the `Docker` calls `uname -r` to get the kernel version. For example, your `uname -r` returns `3.10.0-1127.19.1.el7.x86_64` then the `Makefile` script assumes that the `kernel-devel` RPM must have `3.10.0-1127.19.1` on its name. However, the `kernel-devel` version might differ a bit; instead of `kernel-devel-3.10.0-1127.19.1.el7.x86_64`, it is `kernel-devel-3.10.0-1127.el7.x86_64`.
4. Let's edit the `uname` in the `Docker` to print the correct version as our current `kernel-devel` version. So, make sure you're still inside the `Docker` container (see Step#1).
5. Run these:
```sh
mv /bin/uname /bin/uname.ori
vi /bin/uname
```
6. Then, put this script inside the `/bin/uname`:
```sh
if [ "$1" == "-r" ]; then
    echo "3.10.0-1127.el7.x86_64"
else
    echo "$(uname.ori $1)"
fi
```
7. Finally, make it executable by running: `chmod +x /bin/uname`


### Tested by:

- Aug 31 2021: Mukul Malhotra (mukul.malhotra@seagate.com) on a Windows laptop running VMWare Workstation 16 Pro for CentOS 7.9.2009
- Aug 19 2021: Bo Wei (bo.b.wei@seagate.com) on a Windows laptop running VirtualBox 6.1.
- Aug 18 2021: Jalen Kan (jalen.j.kan@seagate.com) on a Windows laptop running VMWare Workstation 16 Pro.
- July 28 2021: Daniar Kurniawan (daniar@uchicago.edu) on baremetal servers hosted by Chameleon Cloud and Emulab Cloud.
- July 25 2021: Bari Arviv (bari.arviv@seagate.com) on Lyve Labs server - CentOS 7.8.2003.
- July 05 2021: Pranav Sahasrabudhe (pranav.p.shasrabudhe@seagate.com) on a Windows laptop running VMWare Workstation 16 Pro.
