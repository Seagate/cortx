# Installing Dependencies

Follow these steps to install all the dependency packages required to build your components:

1. Download the [CORTX Build Dependencies](https://github.com/Seagate/cortx/releases/tag/build-dependencies) via the Release Branch.
2. Run these commands to install the dependencies:

    ```shell
    
    $ yum install python3-pip createrepo yum-utils -y
    $ pip3 install githubrelease
    $ mkdir -p /root/cortx-build-dependencies && cd /root/cortx-build-dependencies
    $ export GITHUB_TOKEN=<GITHUB TOKEN>
    $ githubrelease --github-token $GITHUB_TOKEN  asset Seagate/cortx download build-dependencies
    $ createrepo -v .
    $ yum-config-manager --add-repo file:///root/cortx-build-dependencies
    $ echo "gpgcheck=0" >> /etc/yum.repos.d/root_cortx-build-dependencies.repo
    $ yum clean all
    ```
    
3. To verify the RPM listing is working, run:

    ```shell
    
    [local_host cortx-build-dependencies]# yum list cortx-py-utils
    ```
    
    **Sample Output**
    
   <img src="../doc/images/Dependencies.png?raw=true">
