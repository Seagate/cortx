* Note current steps are assumed to be run within VM to be configured.

# Setup VM

## Centos 7.5
Import VM from (use latest or check with team)
http://jenkins.mero.colo.seagate.com/share/bigstorage/sage_releases/vmdk_images/

- sage-CentOS-7.5.x86_64-7.5.0_3-k3.10.0.vmdk
-- source:
http://jenkins.mero.colo.seagate.com/share/bigstorage/sage_releases/vmdk_images/sage-CentOS-7.5.x86_64-7.5.0_3-k3.10.0.vmdk

Import VM into VMWare Fusion or VirtualBox

## Centos 7.7
Use the boot.iso ISO from following link to create a VM for Centos 7.7.1908 - kernel 3.10.0-1062 el7
http://eos-poc-katello1.mero.colo.seagate.com/pulp/repos/EOS/Library/custom/CentOS-7/CentOS-7-OS/images/

# Download source
Clone source on the new VM.
  git clone http://gerrit.mero.colo.seagate.com:8080/s3server
or to clone with your seagate gid follow the link given below.
https://docs.google.com/document/d/17ufHPsT62dFL-5VE8r6NeADSefUa0eGgkmPNTkv-k3o/

Ensure you have at least 8GB RAM for dev VM and 4GB RAM for release/rpmbuild VM.
For custom domain configuration see S3 readme for more details.

# To setup dev vm

In case of RHEL/CentOS 8 (Not needed for CentOS 7) Run
script to upgrade packages and enable repos
```sh
cd <s3 src>
./scripts/env/dev/upgrade-enablerepo.sh
```

Run setup script to configure dev vm
```sh
cd <s3 src>
./scripts/env/dev/init.sh
```
* Do NOT use following script directly on Real cluster as it is configured with fully qualified DNS.
For dev VM its safe to run following script to update host entries in /etc/hosts
```sh
./update-hosts.sh
```

Run Build and tests
```sh
./jenkins-build.sh
```

Above will run all system test over HTTPS to run over HTTP specify `--use_http`
```sh
./jenkins-build.sh --use_http
```

# To setup rpmbuild VM and Build S3 rpms
Run setup script to configure rpmbuild vm
```sh
cd <s3 src>
./scripts/env/rpmbuild/init.sh
```
Run following script to update host entries in /etc/hosts
```sh
cd <s3 src>
./update-hosts.sh
```


Install mero/halon
```sh
yum install -y halon mero mero-devel
```
* This installs latest from http://jenkins.mero.colo.seagate.com/share/bigstorage/releases/hermi/last_successful/)

Obtain short git revision to be built.
```sh
git rev-parse --short HEAD
44a07d2
```

Build S3 rpm (here 44a07d2 is obtained from previous git rev-parse command)
./rpms/s3/buildrpm.sh -G 44a07d2

Build s3iamcli rpm
./rpms/s3iamcli/buildrpm.sh -G 44a07d2

Built rpms will be available in ~/rpmbuild/RPMS/x86_64/

This rpms can be copied to release VM for testing.

# To Setup release VM
Run setup script to configure release vm

* Do NOT use this script directly on Real cluster as it cleans existing openldap
setup. You can use steps within init.sh cautiously.
For new/clean VM this is safe.

Ensure you have static hostname setup for the node. Example:
```sh
hostnamectl set-hostname s3release
# check status
hostnamectl status
```

* Do NOT use following script directly on real cluster as it is configured with fully
qualified DNS. Run following script to update host entries in /etc/hosts
```sh
cd <s3 src>
./update-hosts.sh
```

```sh
cd <s3 src>
./scripts/env/release/init.sh
```
VM is now ready to install mero halon s3server s3iamcli and configure.

```sh
yum install -y halon mero s3server s3iamcli s3cmd
```

Once s3server rpm is installed, run following script to update ldap password
in authserver config. [ -l <ldap passwd> -p <authserver.properties file path> ]

/opt/seagate/auth/scripts/enc_ldap_passwd_in_cfg.sh -l ldapadmin \
    -p /opt/seagate/auth/resources/authserver.properties
