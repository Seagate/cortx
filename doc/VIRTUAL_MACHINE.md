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

PRE-BUILD [MOTR]
----------------
[kernel 3.10.0-1062.12.1]
>> uname -r
3.10.0-1062.12.1.el7.x86_64

[install]
>> sudo yum install -y epel-release

>> sudo yum install -y ansible

[verify]
>> rpm -qa | grep ansible

`ansible-2.9.3-1.el7.noarch`

Ensure ansible version is atleast 2.9

Now you are ready to clone, build and/or run motr. Please refer to the [Motr](MeroQuickStart.md) quick start document for further help on this.

PRE-BUILD [S3SERVER]
--------------------
>> Follow the [install] and [verify] steps from motr prebuild steps

>> Next, s3server requires the user to be root. So, change user to root and check the path. Please ensure that the path contains: `/usr/local/bin`. This is required as s3server will install `s3iamcli` utility here, which it requires when running tests, and the path environment variable may not be updated resulting in `s3iamcli` not found error at test runtime.

>> At this point you are ready to build s3server. Please refer to the [s3server](S3ServerQuickStart.md) quick start document on how to do that.

>> Running s3server:

Anytime you run the init.sh script to update build dependencies, you need to rebuild / recompile s3server again. Otherwise, you may see runtime errors.

Authorization errors: if you see the following error when s3server tests are run -
```ERROR: S3 error: 403 (InvalidAccessKeyId): The AWS access key Id you provided does not exist in our records.```
It means that the necessary authorization information is not present. This may happen if you are building the first time and you run basic tests before running the entire test suite. In such a case, you need to run the entire test suite once, this will ensure that the required authorization information is generated and stored for further use.
