# What is RClone?

[video](https://vimeo.com/582062188)

[Rclone](https://github.com/rclone/rclone) ("rsync for cloud storage") is a command line program to sync files and directories to and from different cloud storage providers.

Users call rclone "The Swiss army knife of cloud storage", and "Technology indistinguishable from magic".

Rclone mounts any local, cloud or virtual filesystem as a disk on Windows, macOS, linux and FreeBSD, and also serves these over SFTP, HTTP, WebDAV, FTP and DLNA.

# What is CORTX?

CORTX is a distributed object storage system designed for great efficiency, massive capacity, and high HDD-utilization. CORTX is 100% Open Source.

## How do CORTX and RClone work together?

For large/fast transfer between different cloud storage providers, RClone can make this seamless. RClone is also widely used to have multiple backup across multiple different providers.

With CORTX, this integration with RClone will ease the future integration by consumers who are already utilising multiple cloud providers and would like to add redundancy or additional reliability.

## Configuring RClone to use CORTX as a remote

#### Step 1: Have a CORTX system up and running

If you need instructions on how to set up your own CORTX system you can find instructions on how to setup one on a local machine [here](https://github.com/Seagate/cortx/blob/main/doc/ova/1.0.4/CORTX_on_Open_Virtual_Appliance.rst) or AWS [here.](https://github.com/Seagate/cortx/blob/main/doc/integrations/AWS_EC2.md)

To connect the IFPS client to CORTX you will need these details
* IP ADDRESS
* SECRET KEY
* ACCESS KEY
* BUCKET NAME
* SUBDIRECTORY NAME within BUCKET

> *Note: You will need to write data to CORTX over __http (port 80)__ and __NOT__ https (port 443)*

Check using a tool like [Cyberduck](https://cyberduck.io/) that you can read and write data over http using the details above.

#### Step 2: Create a seperate machine running Linux where you will install the RClone CLI

#### Step 3: Install RClone

```
apt-get install -y rclone
```

#### Step 4: Run rclone configuration

The configuration step is an interactive step - this has been changed across different versions of rclone, but conceptually the same 

```
rclone config
```

```
No remotes found - make a new one
n) New remote
s) Set configuration password
q) Quit config

> n
```

```
name>

> cortx
```

```
Type of storage to configure.
Enter a string value. Press Enter for the default ("").
Choose a number from below, or type in your own value

> s3
```

```
** See help for s3 backend at: https://rclone.org/s3/ **

Choose your S3 provider.
Enter a string value. Press Enter for the default ("").
Choose a number from below, or type in your own value

> other
```

```
Get AWS credentials from runtime (environment variables or EC2/ECS meta data if no env vars).
Only applies if access_key_id and secret_access_key is blank.
Enter a boolean value (true or false). Press Enter for the default ("false").
Choose a number from below, or type in your own value
 1 / Enter AWS credentials in the next step
   \ "false"
 2 / Get AWS credentials from the environment (env vars or IAM)
   \ "true"
env_auth>

> false
```

```
Endpoint for S3 API.
Required when using an S3 clone.
Enter a string value. Press Enter for the default ("").
Choose a number from below, or type in your own value
endpoint> 

> {{Your CORTX Endpoint Here}} 
```

This completes the rclone config

That's it! You connected a remote directory in CORTX using rclone. The next step is optional to test your connection

#### Step 6: Uploading from local disk 

Let's make a file to upload
```
mkdir local_dir
echo "test" > local_dir/test.txt
```

Copying the file from local to remote (Usually rclone is used to copy from remote to remote)
```
rclone copy /local_dir cortx:testbucket -vv --ignore-checksum --s3-v2-auth
```

Copying the file from remote to local
```
rclone copy cortx:testbucket /local_dir -vv --ignore-checksum --s3-v2-auth
```

Tested by:

- Jan 23, 2022: Bo Wei (bo.b.wei@seagate.com) using Cortx OVA 2.0.0 as S3 Server.
