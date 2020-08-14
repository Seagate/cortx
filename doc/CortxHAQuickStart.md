# Cortx-HA QuickStart guide
This is a step by step guide to get Cortx-HA ready for you on your system.
Before cloning, however, you need to have an SSC / Cloud VM or a local VM setup in either VMWare Fusion or Oracle VirtualBox [LocalVMSetup](LocalVMSetup.md).

## Accessing the source code right way
(For phase 1) Latest code which is getting evolved, advancing and contributed is on the current github server.
Seagate contributor will be referencing, cloning and committing code to/from this [Github](https://github.com/Seagate/).

Following steps as sudo user(sudo -s) will make your access to server hassel free.

1. Create SSH Public Key
    * [SSH generation](https://git-scm.com/book/en/v2/Git-on-the-Server-Generating-Your-SSH-Public-Key) will make your key generation super easy. follow the instructions throughly.

2. Add New SSH Public Key on [Github](https://github.com/settings/keys) and [Enable SSO](https://docs.github.com/en/github/authenticating-to-github/authorizing-an-ssh-key-for-use-with-saml-single-sign-on).

WoW! :sparkles:
You are all set to fetch cortx-ha repo now.

## Cloning cortx-ha repository
1. `$ git clone --recursive git@github.com:Seagate/cortx-ha.git` (It has been assumed that `git` is preinstalled. if not then follow git installation specific steps provided [here](https://github.com/Seagate/cortx/blob/master/doc/ContributingToCortxHA.md). Recommended git version is 2.x.x . Check your git version using `$ git --version` command.)

2. `$ git clone --recursive git@github.com:Seagate/cortx-py-utils.git`


## Prerequisites
1. Please make sure python3 and pip3 package are installed on the VM.
    * `$ yum install python36 , python36-devel, openssl-devel, libffi-devel, bzip2-devel,systemd-devel`
    * `$ yum group install "Development Tools"`

2. Install eos-py-utils rpm
    * `$ cd cortx-py-utils`
    * `$ python3 setup.py bdist_rpm`
    * `$ yum install dist/eos-py-utils-1.0.0-1.noarch.rpm`

3. Install pip packages
    * `$ cd cortx-ha`
    * `$ bash jenkins/cicd/cortx-ha-dep.sh dev <github-token>`
    * `$ python3 -m pip install -r jenkins/pyinstaller/requirements.txt`

### corosync pacemaker setup

- Run following command on both node.

1. Disable selinux and firewall on both node.
    * `$ systemctl stop firewalld`
    * `$ systemctl disable firewalld`
    * `$ sestatus`
    * `$ setenforce 0`
    * `$ sed 's/SELINUX=enforcing/SELINUX=disabled/' /etc/sysconfig/selinux`
    * `$ shutdown -r now`
    * `$ getenforce` (It should show disabled)

2. Make sure /etc/hosts is reflected properly or DNS is updated to resolve host names
    * `$ cat /etc/hosts`
      ```
      10.0.15.10      node1
      10.0.15.11      node2

3. Install required Software
    * `$ yum -y install epel-release`
    * `$ yum -y install corosync pacemaker pcs`

4. Configure Pacemaker, Corosync, and Pcsd
    * `$ systemctl enable pcsd`
    * `$ systemctl enable corosync`
    * `$ systemctl enable pacemaker`

  Start pcsd service

    * `$ systemctl start pcsd`

  Configure a password for the 'hacluster' user.
    * `$ echo <new-password> | passwd --stdin hacluster`

5. Create and Configure the Cluster.
    * `$ pcs cluster auth node1 node2`
      ```
      Username: hacluster
      Password:

- Run following command on Primary node.

6. Set up the cluster. Define cluster name and servers that will be part of the cluster.
    * `$ pcs cluster setup --name EOS_cluster node1 node2`

  Now start all cluster services and also enable them.
    * `$ pcs cluster start --all`
    * `$ pcs cluster enable --all`

7. Disable STONITH and Ignore the Quorum Policy
    * `$ pcs property set stonith-enabled=false`
    * `$ pcs property set no-quorum-policy=ignore`
    * `$ pcs property list`

  Check cluster status

    * `$ pcs status cluster`


## Building the CORTX-HA source code
1. Build RPMS
    * Go to the directory where cortx-ha is cloned.
    * `$ jenkins/build.sh` or `$ jenkins/build.sh -b <BUILD-NO>`

2. Install and Configure RPMS
    * `$ yum install -y dist/rpmbuild/RPMS/x86_64/cortx-ha-XXXX.rpm`
    *  From https://github.com/Seagate/cortx-ha/blob/dev/conf/setup.yaml Execute: post_install, config, init and ha.

    Note : To post_install and config we need Cortx Stack or at least salt, pacemaker and consul configured on development box.


## Running Test

To Do

## Running Jenkins / System tests

To Do
