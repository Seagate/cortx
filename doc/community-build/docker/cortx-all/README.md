# Generate CORTX-ALL container image

This document provides step-by-step instructions to build required binaries and generate the CORTX-ALL container image.

## Prerequisites

- Do not update OS or kernel package with yum update as the kernel version must be set to 3.10.0-1160.el7
- Do not upgrade packages from CentOS 7.8 to CentOS 7.9
- Docker >= 20.10.10 . Please refer [Install Docker Community Edition](https://docs.docker.com/engine/install/centos/) steps. Validate docker version on system. 
    ```
    [root@dev-system ~]# docker --version
    Docker version 20.10.8, build 3967b7d
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
    cd /root && git clone https://github.com/Seagate/cortx --recursive --depth=1
    ```
    
2.  Please Checkout **main** branch for generating CORTX packages. Use below command for checkout. 
    ```
    docker run --rm -v /root/cortx:/cortx-workspace ghcr.io/seagate/cortx-build:rockylinux-8.4 make checkout BRANCH=main
    ```
    
    [Optional] You can also checkout **2.0.0-585** from tag instead from **main** branch for generating CORTX packages. Use below command for checkout.
    ```
    docker run --rm -v /root/cortx:/cortx-workspace ghcr.io/seagate/cortx-build:rockylinux-8.4 make checkout BRANCH=2.0.0-585 > /dev/null 2>&1
    ```
     
     - Then check from individual CORTX component repos:
       
       For example:

       ```
       cd cortx/cortx-motr
       git status
       ```

3. Run the following command to build the CORTX packages.
  - For rocky linux use below command:
   ```
   docker run --rm -v /var/artifacts:/var/artifacts -v /root/cortx:/cortx-workspace ghcr.io/seagate/cortx-build:rockylinux-8.4 make clean cortx-all-rockylinux-image cortx-ha
   ```
   
  - For centos use below command:
   ```
   docker run --rm -v /var/artifacts:/var/artifacts -v /root/cortx:/cortx-workspace ghcr.io/seagate/cortx-build:centos-7.9.2009 make clean cortx-all-image cortx-ha
   ```
   
   **Note:** This process takes some time to complete building the CORTX packages during `/var/artifacts/0 /` implementation phase.
   
 
4. Run the following command to generate the ISO for each component:

   ```
   docker run --rm -v /var/artifacts:/var/artifacts -v /root/cortx:/cortx-workspace ghcr.io/seagate/cortx-build:rockylinux-8.4 make iso-generation
   ```
5. To validate that Packages are generated, run the following command after the build step is complete:
   ```
   ll /var/artifacts/0 
   ```

6. (Optional) Compile and Build CORTX Stack as per Individual component.

   Run the following command to view each component targets:
   ```
   docker run ghcr.io/seagate/cortx-build:rockylinux-8.4 make help
   ```
   
7. Publish CORTX release build over HTTP using [Nginx](https://hub.docker.com/_/nginx) docker container. Use below command to create nginx container with required configuration. 
    ```
    docker run --name release-packages-server -v /var/artifacts/0/:/usr/share/nginx/html:ro -d -p 80:80 nginx
    ```

8. We need to change docker compose file and add below extra_hosts for all the services like below. 
    ```
    extra_hosts:
      yourhostname: ipaddress_of_server
    ```
    You can use below command to change it but verify your docker compose before run #9 step.

    ```
    sed -i "/^[[:space:]].*TAG/a\    extra_hosts:\n      - $HOSTNAME: $(hostname -I | cut -d' ' -f1)" docker/cortx-deploy/docker-compose.yml
    ```
    `extra_hosts` entry should be added like below.

    ```
    e.g.  
    cortx-all:
    image: cortx-all:$TAG
    extra_hosts:
      - myhost.example.com: 127.0.0.1
    build:
      context: ./
      dockerfile: ./Dockerfile  
    ```

9. Once docker container is up and running, run the build.sh file where your cortx-all folder is located.
    ```
    docker ps 
    git clone https://github.com/Seagate/cortx-re
    cd cortx-re/docker/cortx-deploy/
    ./build.sh -b http://$HOSTNAME -o rockylinux-8.4 -s all
    ```
    Note: You can use IP Address of system instead of $HOSTNAME if hostname is not reachable. You can find IP address using `ip addr show` command. 

10. Run the below command to see recently generated cortx-all image details.
    ```
    docker images --format='{{.Repository}}:{{.Tag}} {{.CreatedAt}}' cortx-all
    ```
    **Example output** 
    ```
    [root@dev-system ~]# docker images --format='{{.Repository}}:{{.Tag}} {{.CreatedAt}}' cortx-all
    cortx-all:2.0.0-0 2021-11-19 07:31:43 -0700 MST
    ```
### Tested by:
- Feb 08 2022: Amnuay Boottrakoat (amnuay.boottrakoat@seagate.com) on a Windows running VMWare Workstation 16 Player with CentOs 7.9.2009
- Jan 28 2022: Rose Wambui (rose.wambui@seagate.com) on a Windows laptop running VMWare Workstation 16 Pro for CentOs 7.9.2009
- Jan 27 2022: Pranav Sahasrabudhe (pranav.p.sahasrabudhe@seagate.com) on a Mac laptop running VMWare Fusion 16 with CentOs 7.9.2009 VM
- Nov 25 2021: Rose Wambui (rose.wambui@seagate.com) on a Windows laptop running VMWare Workstation 16 Pro for CentOs 7.9.2009
