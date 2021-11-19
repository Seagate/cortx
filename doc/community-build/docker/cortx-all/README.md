# Generate CORTX-ALL container image

This document provides step-by-step instructions to build required binaries and generate the CORTX-ALL container image.

## Prerequisites

- Docker >= 20.10.10 . Please refer [install Docker](https://docs.docker.com/engine/install/centos/) steps. Validate docker version on system. 
    ```
    [root@dev-system ~]# docker --version
    Docker version 20.10.8, build 3967b7d
    ```
 - Docker compose >= 1.29.2 Please refer [install docker compose](https://docs.docker.com/compose/install/) steps. Validate docker-compose version on system.
    ```
    [root@dev-system ~]# docker-compose --version
    docker-compose version 1.29.2, build 5becea4c
    ```
## Procedure

1. Generate community build using steps from [Steps to generate cortx build stack](https://github.com/Seagate/cortx/blob/main/doc/community-build/Generate-Cortx-Build-Stack.md) Please Checkout **kubernetes** branch for generating packages. Use below command for checkout. 
  
    ```
    docker run --rm -v /root/cortx:/cortx-workspace ghcr.io/seagate/cortx-build:centos-7.9.2009 make checkout BRANCH=kubernetes
    ```

2. Validate that Packages are generated at `/var/artifacts/0/` after the build step is complete. 

3. Publish CORTX release build over HTTP using [Nginx](https://hub.docker.com/_/nginx) docker container. Use below command to create nginx container with required configuration. 
    ```
    docker run --name release-packages-server -v /var/artifacts/0/:/usr/share/nginx/html:ro -d -p 80:80 nginx
    ```
4. Once docker container is up and running execute the build.sh file where your cortx-all folder is located.
    ```
    git clone https://github.com/seagate/cortx.git  && cd cortx/doc/community-build/docker/cortx-all/
    ./build.sh -b http://$HOSTNAME
    ```

5. Run the below coammnd to see recently generated cortx-all image details.
    ```
    docker images --format='{{.Repository}}:{{.Tag}} {{.CreatedAt}}' cortx-all
    ```
    **Example output** 
    ```
    [root@dev-system ~]# docker images --format='{{.Repository}}:{{.Tag}} {{.CreatedAt}}' cortx-all
    cortx-all:2.0.0-0 2021-11-19 07:31:43 -0700 MST
    ```
