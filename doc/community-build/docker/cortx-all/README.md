# Generate CORTX-ALL container image

This document provides step-by-step instructions to build required binaries and generate the CORTX-ALL container image.

## Prerequisites

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
## Procedure

1. Run the following command to clone the CORTX repository:
    ```
    cd /root && git clone https://github.com/Seagate/cortx --recursive --depth=1
    ```
    
2.  Please Checkout **kubernetes** branch for generating CORTX packages. Use below command for checkout. 
    ```
    docker run --rm -v /root/cortx:/cortx-workspace ghcr.io/seagate/cortx-build:centos-7.9.2009 make checkout BRANCH=kubernetes
    ```
    
3. Run the following command to build the CORTX packages
   ```
   docker run --rm -v /var/artifacts:/var/artifacts -v /root/cortx:/cortx-workspace ghcr.io/seagate/cortx-build:centos-7.9.2009 make clean cortx-all-image
   ```

5. Validate that Packages are generated at `/var/artifacts/0/` after the build step is complete. 

4. Publish CORTX release build over HTTP using [Nginx](https://hub.docker.com/_/nginx) docker container. Use below command to create nginx container with required configuration. 
    ```
    docker run --name release-packages-server -v /var/artifacts/0/:/usr/share/nginx/html:ro -d -p 80:80 nginx
    ```
5. Once docker container is up and running execute the build.sh file where your cortx-all folder is located.
    ```
    cd /root/cortx/doc/community-build/docker/cortx-all/
    ./build.sh -b http://$HOSTNAME
    ```

6. Run the below coammnd to see recently generated cortx-all image details.
    ```
    docker images --format='{{.Repository}}:{{.Tag}} {{.CreatedAt}}' cortx-all
    ```
    **Example output** 
    ```
    [root@dev-system ~]# docker images --format='{{.Repository}}:{{.Tag}} {{.CreatedAt}}' cortx-all
    cortx-all:2.0.0-0 2021-11-19 07:31:43 -0700 MST
    ```
