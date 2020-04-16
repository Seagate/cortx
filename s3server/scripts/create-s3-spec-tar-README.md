# Setup S3 test environment

### Untar s3-specs.tar
`tar -xvf s3-specs.tar`

### Change dir
`cd s3-specs/st/clitests`

##### _Prior to next step make sure you have installed Python 2.6(required by s3cmd) and Python 3.5. Also install pip_

### Setup virtual environment
`sh setup.sh`
Add following entries to /etc/hosts
```sh
127.0.0.1 seagatebucket.s3.seagate.com seagate-bucket.s3.seagate.com seagatebucket123.s3.seagate.com seagate.bucket.s3.seagate.com
127.0.0.1 s3-us-west-2.seagate.com seagatebucket.s3-us-west-2.seagate.com
127.0.0.1 iam.seagate.com sts.seagate.com
```

### Install s3cmd(S3 client)
`sudo yum --enablerepo epel-testing install s3cmd`

# Run S3 system tests
Make sure fault injection in S3 server is enabled
```sh
cd st/clitests
sh runallsystest.sh
```

# In order to run TCT tests use
```sh
cd st/clitests
python mmcloud_spec.py
```
