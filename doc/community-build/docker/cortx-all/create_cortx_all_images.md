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

    **Note:** If docker and docker compose not present follow the given link to install docker and docker compose. [Install Docker](https://docs.docker.com/engine/install/centos/) [Install docker compose](https://docs.docker.com/compose/install/)
## Procedure

1. Generate community build using steps from [Steps to generate cortx build stack](https://github.com/Seagate/cortx/blob/main/doc/community-build/Generate-Cortx-Build-Stack.md)  . Checkout **kubernetes** branch for generating packages. **NOTE:** Replace 2.0.0-527 to kubernetes while following this document. 
    
2. Validate that Packages are generated at **/var/artifacts/0/** after the build step is complete. 

3. Make cortx package available over HTTP using Nginx docker image using below command.
    ```
    docker run --name test-nginx -v /var/artifacts/0/:/usr/share/nginx/html:ro -d -p 80:80 nginx
    ```
4. Once docker container is up and running execute the build.sh file where your cortx-all folder is located.
    ```
    cd ~/cortx/tree/cortx-all-image/doc/community-build/docker/cortx-all
    ```
    ```
    ./build.sh <URL_PATH_TO_ACCESS_ARTIFACTS>
    ```
5. Run the below coammnd to see cortx-all images
    ```
    docker images
    ```

