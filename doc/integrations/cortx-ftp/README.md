# CORTX Integration with FTP

What is FTP
----
The File Transfer Protocol (FTP) is a standard communication protocol used for the transfer of computer files from a server to a client on a computer network. 

Integrating CORTX with FTP
----
The integration architecture is shown below. In this integration, there is a FTP client, a FTP server, and a Cortx/S3 server. FTP client and FTP server connect through FTP protocol. FTP server uses s3fs mount as storage. s3fs sync with S3 server's S3 bucket.    

Other S3 client tools can be added to the system to manage S3 storage, or use to test the system.  

![architecture](image/ftp_integration_architecture.jpg)

###  Prerequisites
Have an avaliable Cortx setup, or setup follow the instructions below. And create a bucket testbucket.
https://github.com/Seagate/cortx/blob/main/QUICK_START.md 

###  Setup Steps 
Note: Dockerfile is available to replace Step 1 and Step 2. 

Step 1: Install s3fs

    apt-get install -y s3fs


Step 2: Install ftp server

    apt-get install -y vsftpd
    sed -i 's/anonymous_enable=NO/anonymous_enable=YES/' /etc/vsftpd.conf
    mkdir /srv/ftp/cortx-fs
    chmod 600 /srv/ftp/cortx-fs
    service vsftpd start

Step 3: Mount S3 bucket with s3fs. Note: replace with your own access key id, secret access key and url.

    echo your_access_key_id:your_secret_access_key > /etc/passwd-s3fs
    chmod 600 /etc/passwd-s3fs
    s3fs testbucket /srv/ftp/cortx-fs/ -o passwd_file=/etc/passwd-s3fs -o url=https://192.168.1.111:443 -o use_path_request_style -o dbglevel=info -f -o curldbg -o ssl_verify_hostname=0 -o no_check_certificate -o allow_other -o complement_stat -o umask=600


###  Run and test the system 

Step 1: Add some files to the testbucket using tools like Cyberduck or awscli (need to setup separately). 

To install awscli.

    apt-get install -y awscli

Create aws credential file at ~/.aws/credentials with the following contents.

    [default]
    aws_access_key_id = your_access_key_id
    aws_secret_access_key = your_secret_access_key

Create test file in Cortx S3 bucket.

    echo "test_ftp" > test_file
    aws --endpoint "https://192.168.1.111:443" --no-verify-ssl s3 cp ./test_file s3://testbucket/

Step 2: Use ftp client on another machine (e.g. Windows has default ftp client available) to connect to ftp server (use your own ftp server ip). Login with Name: ftp and Password is empty. List and get the test_file.

    c:\tmp>ftp 172.27.117.60
    Connected to 172.27.117.60.
    220 (vsFTPd 3.0.3)
    200 Always in UTF8 mode.
    User (172.27.117.60:(none)): ftp
    331 Please specify the password.
    Password:
    230 Login successful.
    ftp> ls
    200 PORT command successful. Consider using PASV.
    150 Here comes the directory listing.
    cortx-fs
    226 Directory send OK.
    ftp: 13 bytes received in 0.00Seconds 13.00Kbytes/sec.
    ftp> cd cortx-fs
    250 Directory successfully changed.
    ftp> ls
    200 PORT command successful. Consider using PASV.
    150 Here comes the directory listing.
    test_file
    226 Directory send OK.
    ftp: 14 bytes received in 0.03Seconds 0.48Kbytes/sec.
    ftp> get test_file
    200 PORT command successful. Consider using PASV.
    150 Opening BINARY mode data connection for test_file (9 bytes).
    226 Transfer complete.
    ftp: 9 bytes received in 0.03Seconds 0.26Kbytes/sec.

    c:\tmp>type test_file
    test_ftp

All done. Now you have a ftp server ready with Cortx as its storage.
