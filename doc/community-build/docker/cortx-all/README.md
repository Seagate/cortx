# Generate CORTX container images

This document provides step-by-step instructions to build CORTX binaries and container images.

## Prerequisites
- Create Linux VM with CentOS 7.9 or Rocky Linux 8.4 with below minimum requirements.
    - RAM: 8GB
    - Processor: 4

- Ensure following minimum disk space are available. 
    - 70GB : Docker Home Directory where default docker home directory is /var/lib/docker. You can modify the file based on disk layout.
    - 30GB : mnt . Use this directory as part of build process. You can use /mnt from system disk if you have enough disk space available.

- Git >= 2.27.0 . Please refer [Install Git](https://git-scm.com/book/en/v2/Getting-Started-Installing-Git) document. You can also use below commmand to install Git. 
  ```
  yum install git -y 
  ```

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
    
## Compile and Build CORTX Stack

- Pull CORTX Build image:
     ```
     docker pull ghcr.io/seagate/cortx-build:rockylinux-8.4
     ```

## Procedure

1. Run the following command to clone the CORTX repository:
    ```
    cd /mnt && git clone https://github.com/Seagate/cortx --recursive --depth=1
    ```
    
2.  Run the following command to checkout **main** branch for generating CORTX packages. Use below command for checkout. 
    ```
    docker run --rm -v /mnt/cortx:/cortx-workspace ghcr.io/seagate/cortx-build:rockylinux-8.4 make checkout BRANCH=main
    ```
     
     - Validate CORTX component clone status with below command.
       
       ```
       cd /mnt/cortx/ && for component in cortx-motr cortx-hare cortx-rgw-integration cortx-manager cortx-utils cortx-ha cortx-rgw; do echo -e "\n==[ Checking Git Branch for $component ]=="; pushd $component; git status; popd ; done && cd -
       ```

3. Run the following command to build the CORTX packages.
   ```
   docker run --rm -v /var/artifacts:/var/artifacts -v /mnt/cortx:/cortx-workspace ghcr.io/seagate/cortx-build:rockylinux-8.4 make clean cortx-all-rockylinux-image
   ```
   
   **Note:** This process takes around 5Hrs to build entire CORTX packages with minimum VM specifications where time can vary based on VM specification and network speed.
   
4. To validate that Packages are generated, run the following command after the build step is complete:
   ```
   ls -l /var/artifacts/0 
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
    Ensure that container is started and we can download RELEASE.INFO
    ```
    docker ps 
    curl -L http://$HOSTNAME/RELEASE.INFO
    ```
    

7. Run the following command to clone [cortx-re](https://github.com/Seagate/cortx-re) repository using below commands,

    ```
    git clone https://github.com/Seagate/cortx-re && cd cortx-re/docker/cortx-deploy/
    ```
    - We need to add extra_hosts parameter in docker-compose.yml for all the services to run build.sh using $HOSTNAME as below
    ```
    extra_hosts:
      - "yourhostname: ipaddress_of_server"
    ```
    - You can use below command to change it but verify your docker compose before run 8 step.

    ```
    sed -i "/^[[:space:]].*TAG/a\    extra_hosts:\n      - \"$HOSTNAME: $(hostname -I | cut -d' ' -f1)"\" docker-compose.yml
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

    - Run below command to build cortx-all image. Ensure you are in `cortx-re/docker/cortx-deploy/` directory:
    ```
    ./build.sh -b http://$HOSTNAME -o rockylinux-8.4 -s all -e opensource-ci
    ```
    Note: You can use IP Address of system instead of $HOSTNAME if hostname is not reachable. You can find IP address using `ip addr show` command. 

9. Run the below command to see recently generated cortx-all image details.
    ```
    docker images --format='{{.Repository}}:{{.Tag}} {{.CreatedAt}}' --filter=reference='cortx-*'
    ```
    **Example output** 
    ```
    [root@dev-system ~]#  docker images --format='{{.Repository}}:{{.Tag}} {{.CreatedAt}}' --filter=reference='cortx-*'
    cortx-rgw:2.0.0-0 2022-04-20 08:40:27 -0600 MDT
    cortx-data:2.0.0-0 2022-04-20 08:39:46 -0600 MDT
    cortx-all:2.0.0-0 2022-04-20 08:39:46 -0600 MDT
    cortx-control:2.0.0-0 2022-04-20 08:38:37 -0600 MDT
    ```
### Tested by:

- Mar 23 2022: Abhijit Patil (abhijit.patil@seagate.com) on a AWS EC2 instance with RockyLinux 8.5
- Feb 10 2022: Bo Wei (bo.b.wei@seagate.com) on a Windows running VirtualBox with CentOs 7.9.2009
- Feb 08 2022: Amnuay Boottrakoat (amnuay.boottrakoat@seagate.com) on a Windows running VMWare Workstation 16 Player with CentOs 7.9.2009
- Jan 28 2022: Rose Wambui (rose.wambui@seagate.com) on a Windows laptop running VMWare Workstation 16 Pro for CentOs 7.9.2009
- Jan 27 2022: Pranav Sahasrabudhe (pranav.p.sahasrabudhe@seagate.com) on a Mac laptop running VMWare Fusion 16 with CentOs 7.9.2009 VM
- Nov 25 2021: Rose Wambui (rose.wambui@seagate.com) on a Windows laptop running VMWare Workstation 16 Pro for CentOs 7.9.2009
