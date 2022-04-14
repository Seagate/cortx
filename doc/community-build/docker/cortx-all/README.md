# Generate CORTX container images

This document provides step-by-step instructions to build required binaries and generate the CORTX-ALL container image.

## Prerequisites

- Do not update OS or kernel package with yum update as the kernel version must be set to 3.10.0-1160.el7
- Do not upgrade packages from CentOS 7.8 to CentOS 7.9
- Before installing docker, docker home directory space should be 70GB (default docker home directory is /var/lib/docker) and /mnt drive space should be 30GB to run cortx build.
- Docker >= 20.10.14 . Please refer [Install Docker Community Edition](https://docs.docker.com/engine/install/centos/) steps. Validate docker version on system. 
    ```
    [root@dev-system ~]# docker --version
    Docker version 20.10.14, build a224086
    ```
 - Docker compose >= 1.29.2 Please refer [Install docker compose](https://docs.docker.com/compose/install/) steps. Validate docker-compose version on system.
    ```
    [root@dev-system ~]# docker-compose --version
    docker-compose version 1.29.2, build 5becea4c
    ```
    
## Compile and Build CORTX Stack from HEAD

- Run the appropriate tag as per OS required i.e. CentOS 7.8, CentOS 7.9 or rockylinux 8. For example:

   - For CentOS 7.8.2003:
     ```
     docker pull ghcr.io/seagate/cortx-build:centos-7.8.2003
     ```
   - For CentOS 7.9.2009:
     ```
     docker pull ghcr.io/seagate/cortx-build:centos-7.9.2009
     ```
   - For rockylinux 8.4:
     ```
     docker pull ghcr.io/seagate/cortx-build:rockylinux-8.4
     ```

## Procedure

1. Run the following command to clone the CORTX repository:
     ```
     cd /mnt && git clone https://github.com/Seagate/cortx --recursive --depth=1 && cd /mnt/cortx && git clone https://github.com/Seagate/cortx-rgw
     ```
    
2. Run the following command to checkout the codebase from **main** branch for generating CORTX packages: 
     ```
     docker run --rm -v /mnt/cortx:/cortx-workspace ghcr.io/seagate/cortx-build:rockylinux-8.4 make checkout BRANCH=main
     ```
    
   - Then check from individual CORTX component repos:
        
     For example:
     ```
     cd /mnt/cortx/cortx-motr
     git status
     ```

3. Run the following command to build the CORTX packages for rocky linux:
     ```
     docker run --rm -v /var/artifacts:/var/artifacts -v /mnt/cortx:/cortx-workspace ghcr.io/seagate/cortx-build:rockylinux-8.4 make clean cortx-all-rockylinux-image cortx-ha
     ```
   
   **Note:** This process takes some time to complete building the CORTX packages during `/var/artifacts/0 /` implementation phase.
   
4. To validate that Packages are generated, run the following command after the build step is complete:
     ```
     ll /var/artifacts/0 
     ```

5. (Optional) Compile and Build CORTX Stack as per Individual component.

   Run the following command to view each component targets:
     ```
     docker run ghcr.io/seagate/cortx-build:rockylinux-8.4 make help
     ```
   
6. Publish CORTX release build over HTTP using [Nginx](https://hub.docker.com/_/nginx) docker container. Use below command to create nginx container with required configuration. 

     ```
     docker run --name release-packages-server -v /var/artifacts/0/:/usr/share/nginx/html:ro -d -p 80:80 nginx
     ```

7. We need to clone cortx repo, use below commands for same.

     ```
     docker ps 
     curl -L http://$HOSTNAME/RELEASE.INFO
     git clone https://github.com/Seagate/cortx-re && cd cortx-re/docker/cortx-deploy/
     ```

    - If you run build.sh by $HOSTNAME then here we need change docker-compose.yml and add below extra_hosts in that docker compose for all the services like below.
    ```
    extra_hosts:
      - "yourhostname: ipaddress_of_server"
    ```
    - You can use below command to change it but verify your docker compose before run 8 step.

    ```
    sed -i "/^[[:space:]].*TAG/a\    extra_hosts:\n      - \"$HOSTNAME: $(hostname -I | cut -d' ' -f1)"\" docker/cortx-deploy/docker-compose.yml
    ```
    `extra_hosts` entry should be added like below.

    ```
    e.g.  
    cortx-all:
    image: cortx-all:$TAG
    extra_hosts:
      - "myhost.example.com: 127.0.0.1"
    build:
      context: ./
      dockerfile: ./Dockerfile  
    ```

8. After verifying docker compose then run the build.sh file where your cortx-all folder is located.

    - Use below command to build cortx-all image using rocky linux:
    ```
    ./build.sh -b http://$HOSTNAME -o rockylinux-8.4 -s all -e opensource-ci
    ```
    - Use below command to build cortx-all image using centos:
    ```
    ./build.sh -b http://$HOSTNAME -o centos-7.9.2009 -e opensource-ci
    ```
    **Note:** You can use IP Address of system instead of $HOSTNAME if hostname is not reachable. You can find IP address using `ip addr show` command. 

9. Run the below command to see recently generated cortx-all image details.
    ```
    docker images --format='{{.Repository}}:{{.Tag}} {{.CreatedAt}}'|grep cortx|grep -v cortx-build
    ```
    **Example output:** 
    ```
    [root@dev-system ~]# docker images --format='{{.Repository}}:{{.Tag}} {{.CreatedAt}}'|grep cortx|grep -v cortx-build
    cortx-data:2.0.0-0 2022-03-23 11:45:02 -0700 MST
    cortx-rgw:2.0.0-0 2022-03-23 11:45:31 -0700 MST
    cortx-all:2.0.0-0 2022-03-23 11:45:23 -0700 MST
    cortx-control:2.0.0-0 2022-03-23 11:51:23 -0700 MST
    ```
### Tested by:

- April 13 2022: Mukul Malhotra (mukul.malhotra@seagate.com) on a VMWare vSphere 7.0.1 for CentOS 7.9.2009
- Mar 23 2022: Abhijit Patil (abhijit.patil@seagate.com) on a AWS EC2 instance with RockyLinux 8.5
  - On AWS EC2 we don't have rocky linux 8.4 AMI, so I decided to use RockyLinux 8.5 and then I follow above steps.
- Feb 10 2022: Bo Wei (bo.b.wei@seagate.com) on a Windows running VirtualBox with CentOs 7.9.2009
- Feb 08 2022: Amnuay Boottrakoat (amnuay.boottrakoat@seagate.com) on a Windows running VMWare Workstation 16 Player with CentOs 7.9.2009
- Jan 28 2022: Rose Wambui (rose.wambui@seagate.com) on a Windows laptop running VMWare Workstation 16 Pro for CentOs 7.9.2009
- Jan 27 2022: Pranav Sahasrabudhe (pranav.p.sahasrabudhe@seagate.com) on a Mac laptop running VMWare Fusion 16 with CentOs 7.9.2009 VM
- Nov 25 2021: Rose Wambui (rose.wambui@seagate.com) on a Windows laptop running VMWare Workstation 16 Pro for CentOs 7.9.2009
