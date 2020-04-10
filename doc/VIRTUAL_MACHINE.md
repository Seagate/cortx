CentOS 7.7 dev VM
=================

ISO
---
download CentOS 7.7.1908 ISO.
http://mirrors.ukfast.co.uk/sites/ftp.centos.org/7.7.1908/isos/x86_64/
http://mirrors.ukfast.co.uk/sites/ftp.centos.org/7.7.1908/isos/x86_64/CentOS-7-x86_64-DVD-1908.iso

VM
--
Create VM.
Minimum requirements:
CPU = 4, Memory = 8GB, Storage = 64GB
Perhaps these requirements can be lowered.
Install CentOS 7.7.1908 ISO.


RPMs
----

[kernel 3.10.0-1062.12.1]
>> uname -r
3.10.0-1062.12.1.el7.x86_64

[install]
>> sudo yum install epel-release.noarch
>> sudo yum-config-manager --enable updates extras epel


[update exclude kernel]
>> sudo yum update --exclude=kernel*
>> reboot

[rpms]
>> ls
kmod-lustre-client-2.12.3-1.el7.x86_64.rpm
lustre-client-2.12.3-1.el7.x86_64.rpm
lustre-client-devel-2.12.3-1.el7.x86_64.rpm
mero-1.4.0-B69876_git24d95a8ad_3.10.0_1062.12.1.el7.x86_64.rpm
mero-devel-1.4.0-B69876_git24d95a8ad_3.10.0_1062.12.1.el7.x86_64.rpm


[install]
>> sudo yum install ./kmod-lustre-client-2.12.3-1.el7.x86_64.rpm ./lustre-client-2.12.3-1.el7.x86_64.rpm ./lustre-client-devel-2.12.3-1.el7.x86_64.rpm 
>> sudo yum install ./mero-*

[verify]
>> rpm -qa | grep mero-
mero-devel-1.4.0-B69876_git24d95a8ad_3.10.0_1062.12.1.el7.x86_64
mero-1.4.0-B69876_git24d95a8ad_3.10.0_1062.12.1.el7.x86_64
>> rpm -qa | grep lustre
lustre-client-2.12.3-1.el7.x86_64
kmod-lustre-client-2.12.3-1.el7.x86_64
lustre-client-devel-2.12.3-1.el7.x86_64
[seagate@localhost ~]$ 

[configure Lustre]
>> sudo su
# replace eth0 with your vm's interface
>> echo 'options lnet networks=tcp(eth0) config_on_load=1' > /etc/modprobe.d/lnet.conf
>> echo '---' >> /etc/lnet.conf
>> systemctl start lnet
>> lctl list_nids
>> exit

########################################
Single Node Mero
########################################

>> sudo rm -rf /etc/mero/ /var/mero
>> sudo m0singlenode activate
>> sudo m0setup -cv
>> sudo m0setup -Mv
>> sudo m0singlenode start
>> sudo m0singlenode stop

[m0t1fs]
>> sudo m0singlenode start
>> cd /mnt/m0t1fs/
>> sudo touch /mnt/m0t1fs/0:3000
## sudo setfattr -n lid -v 8 /mnt/m0t1fs/0:3000
>> sudo dd if=/dev/zero of=/mnt/m0t1fs/0:3000 bs=4M count=10
10+0 records in
10+0 records out
41943040 bytes (42 MB) copied, 0.64216 s, 65.3 MB/s
>> ls -lh /mnt/m0t1fs/0:3000
----------. 1 root root 40M Mar 23 03:23 /mnt/m0t1fs/0:3000

>> sudo rm -rf /mnt/m0t1fs/0:3000
>> ls -lh /mnt/m0t1fs/0:3000
ls: cannot access /mnt/m0t1fs/0:3000: No such file or directory
>> sudo touch /mnt/m0t1fs/0:3000
>> ls -lh /mnt/m0t1fs/0:3000
-rw-r--r--. 1 root root 0 Mar 23 03:26 /mnt/m0t1fs/0:3000
>> sudo dd if=/dev/zero of=/mnt/m0t1fs/0:3000 bs=4M count=20
20+0 records in
20+0 records out
83886080 bytes (84 MB) copied, 1.35209 s, 62.0 MB/s
>> ls -lh /mnt/m0t1fs/0:3000
-rw-r--r--. 1 root root 80M Mar 23 03:27 /mnt/m0t1fs/0:3000

[non-sudo]
>> sudo usermod -aG mero seagate
>> cat /etc/group | grep mero
mero:x:980:seagate
>> sudo m0singlenode stop
>> reboot
>> sudo m0singlenode start

[c0appz]
>> tar -zxvf /mnt/hgfs/linux/rpms/CSA-15112019.tar.gz 
>> cd clovis-sample-apps/
>> make vmrcf
mkdir -p .c0cprc
mkdir -p .c0ctrc
mkdir -p .c0rmrc
mkdir -p .c0cp_asyncrc
./scripts/c0appzrcgen > ./.c0cprc/localhost.localdomain
./scripts/c0appzrcgen > ./.c0ctrc/localhost.localdomain
./scripts/c0appzrcgen > ./.c0rmrc/localhost.localdomain
./scripts/c0appzrcgen > ./.c0cp_asyncrc/localhost.localdomain
>> make
>> make test
>> make clean

