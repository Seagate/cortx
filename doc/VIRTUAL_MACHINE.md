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
Install CentOS 7.7.1908 ISO.

PRE-BUILD
---------
[kernel 3.10.0-1062.12.1]
>> uname -r
3.10.0-1062.12.1.el7.x86_64

[install]
>> sudo yum install -y epel-release

>> sudo yum install -y ansible

[verify]
>> rpm -qa | grep ansible
ansible-2.9.3-1.el7.noarch

Ensure version is atleast 2.9

Now you are ready to clone, build and/or run Mero. Please refer to the [Mero](MeroQuickStart.md) quick start document for further help on this.
