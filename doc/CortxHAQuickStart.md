# Cortx-HA QuickStart guide
This is a step by step guide to get Cortx-HA ready for you on your system.
Before cloning, however, you need to have an SSC / Cloud VM or a local VM setup in either VMWare Fusion or Oracle VirtualBox [LocalVMSetup](LocalVMSetup.md). If you prefer video, here is a [link to a video](https://seagatetechnology.sharepoint.com/:v:/s/gteamdrv1/tdrive1224/EZbJ5AUWe79DksiRctCtsnUB9sILRr5DqHeBzdrwzNNg6w?e=Xamvex) produced by Seagate engineer Puja Mudaliar following these instructions.

## Accessing the source code right way
(For phase 1) Latest code which is getting evolved, advancing and contributed is on the current github server.
Seagate contributor will be referencing, cloning and committing code to/from this [Github](https://github.com/Seagate/).

Following steps as sudo user(sudo -s) will make your access to server hassel free.


1. Create and Add SSH Public Key:
    * For creating and adding SSH key please follow refer "Github setup" section from [ContributingToCortxHA](https://github.com/Seagate/cortx/blob/master/doc/ContributingToCortxHA.md#GitHub-setup)
    * You can also refer [SSH generation](https://git-scm.com/book/en/v2/Git-on-the-Server-Generating-Your-SSH-Public-Key).

WoW! :sparkles:
You are all set to fetch cortx-ha repo now.

## Prerequisites
1. Setup Yum repos.
    * `$ curl https://raw.githubusercontent.com/Seagate/cortx-prvsnr/dev/cli/src/cortx-prereqs.sh?token=APA75GY34Y2F5DJSOKDCZAK7ITSZC -o cortx-prereqs.sh; chmod a+x cortx-prereqs.sh`
    * For Cent-OS VMs : `$ sh cortx-prereqs.sh`, For Rhel VMs: `$ sh cortx-prereqs.sh --disable-sub-mgr`

2. Please make sure python3,pip3 and kernel-devel-3.10.0-1062 packages are installed on the VM.
    * `$ yum install python36`
    * `$ yum install python36-devel`
    * `$ yum install openssl-devel`
    * `$ yum install libffi-devel`
    * `$ yum install bzip2-devel`
    * `$ yum install systemd-devel`
    * `$ yum group install "Development Tools"`

3. Install eos-py-utils rpm
    * `$ git clone --recursive git@github.com:Seagate/cortx-py-utils.git`
    * `$ cd cortx-py-utils`
    * `$ python3 setup.py bdist_rpm`
    * `$ yum install dist/eos-py-utils-1.0.0-1.noarch.rpm`

### corosync pacemaker setup

  Note: For corosync pacemaker setup we need two VM's. Run step 1-5 below, on both nodes and other step on primary node node.

- Run following command on both nodes.

1. Setup Yum repos on both nodes.
    * Refere [Setup Yum repos](#Prerequisites)

2. Disable selinux and firewall on both node.
    * `$ systemctl stop firewalld`
    * `$ systemctl disable firewalld`
    * `$ sestatus`
    * `$ setenforce 0`
    * `$ sed 's/SELINUX=enforcing/SELINUX=disabled/' /etc/sysconfig/selinux`
    * `$ shutdown -r now`
    * `$ getenforce` (It should show disabled)

3. Make sure /etc/hosts is reflected properly or DNS is updated to resolve host names

    Add eth0 ip address of both nodes in /etc/hosts file.

    * `$ cat /etc/hosts`
      ```
      10.0.15.10      node1
      10.0.15.11      node2

4. Install required Software
    * `$ yum -y install corosync pacemaker pcs`

5. Configure Pacemaker, Corosync, and Pcsd
    * `$ systemctl enable pcsd`
    * `$ systemctl enable corosync`
    * `$ systemctl enable pacemaker`

    Start pcsd service

    * `$ systemctl start pcsd`

    Configure a password for the 'hacluster' user.

    * `$ echo <new-password> | passwd --stdin hacluster`

    Example : echo testDoc | passwd --stdin hacluster

- Run following command on Primary node.

6. Create and Configure the Cluster.
    * `$ pcs cluster auth node1 node2`
      ```
      Username: hacluster
      Password:

7. Set up the cluster. Define cluster name and servers that will be part of the cluster.
    * `$ pcs cluster setup --name EOS_cluster node1 node2`

    Now start all cluster services and also enable them.

    * `$ pcs cluster start --all`

    * `$ pcs cluster enable --all`

8. Disable STONITH and Ignore the Quorum Policy
    * `$ pcs property set stonith-enabled=false`
    * `$ pcs property set no-quorum-policy=ignore`
    * `$ pcs property list`

    Example output:
    ~~~
    [root@ssc-vm-c-0208 534380]# pcs property list
    Cluster Properties:
    cluster-infrastructure: corosync
    cluster-name: EOS_cluster
    dc-version: 1.1.21-4.el7-f14e36fd43
    have-watchdog: false
    no-quorum-policy: ignore
    stonith-enabled: false
    ~~~

    Check cluster status

    * `$ pcs status cluster`

    Example output:
    ~~~
    [root@ssc-vm-c-0208 534380]# pcs status cluster
    Cluster Status:
    Stack: corosync
    Current DC: node1 (version 1.1.21-4.el7-f14e36fd43) - partition with quorum
    Last updated: Wed Aug 19 02:04:43 2020
    Last change: Wed Aug 19 02:03:57 2020 by root via cibadmin on node1
    2 nodes configured
    0 resources configured

    PCSD Status:
    node2: Online
    node1: Online
    ~~~

## Cloning cortx-ha repository
1. `$ git clone --recursive git@github.com:Seagate/cortx-ha.git` (It has been assumed that `git` is preinstalled. if not then follow git installation specific steps provided [here](https://github.com/Seagate/cortx/blob/master/doc/ContributingToCortxHA.md). Recommended git version is 2.x.x . Check your git version using `$ git --version` command.)

2. `$ cd cortx-ha`


## Building the CORTX-HA source code

1. Install pip packages
    * `$ bash jenkins/cicd/cortx-ha-dep.sh dev <github-token>`

    Example : bash jenkins/cicd/cortx-ha-dep.sh dev 9a876db873420e5d6aabbcc7896eb234a567890

    Note : For creating github token follow steps from [ContributingToCortxHA](https://github.com/Seagate/cortx/blob/master/doc/ContributingToCortxHA.md#token-personal-access-for-command-line-required-for-submodule-clone-process)

    * `$ python3 -m pip install -r jenkins/pyinstaller/requirements.txt`

2. Build RPMS
    * Go to the directory where cortx-ha is cloned.
    * `$ jenkins/build.sh` **OR** `$ jenkins/build.sh -b <BUILD-NO>`

3. Install and Configure RPMS
    * `$ yum install -y dist/rpmbuild/RPMS/x86_64/cortx-ha-XXXX.rpm`

    Example : yum install -y dist/rpmbuild/RPMS/x86_64/cortx-ha-1.0.0-368034b.x86_64.rpm 

    Check CICD if return non zero then exit and fail build.

    * `$ bash jenkins/cicd/cortx-ha-cicd.sh`

    *  From https://github.com/Seagate/cortx-ha/blob/dev/conf/setup.yaml Execute: post_install, config, init and ha.

    Note :
    1. To configure HA we need Cortx Stack or at least salt, pacemaker and consul configured on the development box.
    2. Currently HA will support only on the hardware.

## Running Test
  * `$ cd cortx-ha/ha/test/`
  * `$ python3 main.py`
