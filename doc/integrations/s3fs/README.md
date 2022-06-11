# Running CORTX with s3fs

[Video link](https://vimeo.com/581753953)


Summary:
----
This document walks you through how to set up CORTX integration with [s3fs](https://github.com/s3fs-fuse/s3fs-fuse). s3fs allows Linux and macOS to mount an S3 bucket via FUSE. s3fs preserves the native object format for files, allowing use of other tools like AWS CLI.

This documentations runs the s3fs + CORTX integration with a Docker instance as well. 



Step 1: Replace Credentials
--------

- Head over to two files (```aws_creds``` and ```creds```) and replace the existing ACCESS KEY and SECRET KEY with your own keys



Step 2: Build Dockerfile
--------

For windows 10, run the following command.

Note: To run `Get-Content` command in Windows 10, need to use Windows PowerShell.

To run `docker` command in Windows 10, need to install WSL2 and Docker Desktop [here](https://www.docker.com/products/docker-desktop) first. 
```
Get-Content Dockerfile | docker build -t cortx .
```

Step 3: Run Docker
------
```
docker run -t -d --cap-add SYS_ADMIN --device /dev/fuse cortx
```

Step 4: Attach CORTX via s3fs
------

Enter the following command while ssh-ed or inside the docker instance.

Note: if you use development Cortx S3 Server, and encounter "server certificate verification" error, you can add `-o no_check_certificate` to avoid the error.

If you encouter "s3fs: unable to access MOUNTPOINT cortx-fs: No such file or directory", you can `cd /` first then run this s3fs command.
```
s3fs testbucket cortx-fs -o passwd_file=${HOME}/.passwd-s3fs -o url=http://uvo10yvtzaut5d6y06l.vm.cld.sr -o use_path_request_style 
```

If you do not have testbucket created in cortx, you may do so via the awscli.

```
aws s3api create-bucket --bucket testbucket --endpoint "http://uvo10yvtzaut5d6y06l.vm.cld.sr"
```



Step 5 (Optional): Test out the file system by creating a file or downloading a file
------

Following the video guide, we can try to download a big file and see the changes in our system

```
cd testbucket
wget https://github.com/Seagate/cortx/archive/refs/tags/cortx-ova-2.0.0-264.zip
```

Tested By:

- Dec 11, 2021: Bo Wei (bo.b.wei@seagate.com) using Cortx OVA 2.0.0 as S3 Server, Docker Desktop as docker instance for s3fs.
- Nov 30, 2021: Harrison Seow (harrison.seow@seagate.com) using cortx-va-1.0.3 running on CloudShare, client on Linux WSL2.
