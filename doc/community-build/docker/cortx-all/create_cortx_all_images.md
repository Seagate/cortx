# Generate CORTX-ALL image for the community (outside Seagate network)

This document provides step-by-step instructions to build and generate the CORTX image.

## Prerequisites

- latest version of docker and docker compos.
    -   Docker Version
    ```
    docker --version
    Docker version 20.10.10
    ```
    - Docker compose Version
    ```
    docker-compose --version
    docker-compose version 1.29.2
    ```

    **Note:** If docker(docker-ce) and docker compose not present follow the given link to install docker and docker compose. [Install Docker](https://docs.docker.com/engine/install/centos/) [Install docker compose](https://docs.docker.com/compose/install/)
## Procedure

1. Generate community build using steps from [Steps to generate cortx build stack](https://github.com/Seagate/cortx/blob/main/doc/community-build/Generate-Cortx-Build-Stack.md)  . Checkout **kubernetes** branch for generating packages. **NOTE:** Replace ***2.0.0-527*** to ***kubernetes*** while following this document.
    **Example**
    ```
    docker run --rm -v /root/cortx:/cortx-workspace ghcr.io/seagate/cortx-build:centos-7.9.2009 make checkout BRANCH=kubernetes > /dev/null 2>&1
    cd ~/cortx/cortx-s3server; git checkout kubernetes
    ```

2. Validate that Packages are generated at ***/var/artifacts/0/*** after the build step is complete. 

3. Make cortx package available over HTTP using Nginx docker image using below command.
    ```
    docker run --name Release-packages-server -v /var/artifacts/0/:/usr/share/nginx/html:ro -d -p 80:80 nginx
    ```
4. Once docker container is up and running execute the build.sh file where your cortx-all folder is located.
    ```
    git clone -b <BRANCH_NAME> https://github.com/<REPO_NAME>/cortx.git  && cd cortx/doc/community-build/docker/cortx-all/
    ```
    ```
    ./build.sh <URL_PATH_TO_ACCESS_ARTIFACTS>
    ```
    **Example** 
    ``` 
    git clone -b cortx-all-image https://github.com/nikhilpatil2995/cortx.git  && cd cortx/doc/community-build/docker/cortx-all/
    ./build.sh -b http://ssc-vm-rhev4-0707.colo.seagate.com
    ```

5. Run the below coammnd to see cortx-all images
    ```
    docker images
    ```
    **Example** 
    ```
    [root@ssc-vm-g3-rhev4-2461 community-build]# docker images  | grep cortx-all
    cortx-all                     2.0.0-0-main             6c81b35aa92f   27 hours ago   1.91GB
    ```
