CORTX OVA: Performing S3 IO Operations
======================================


## Prerequisites

- Install the AWSCLI packages.
```bash
pip3 install awscli
pip3 install awscli-plugin-endpoint
```

- Import OVA on hypervisor i.e. VMware Workstation and wait for 5 to 10 mins during the booting stage till the login screen comes.


## Procedure

- Run the following commands:
```bash
mkdir -p /var/log/cortx/auth
touch /var/log/cortx/auth/s3iamcli.log
kubectl get pods -o wide | grep cortx-data
```

- Configure the default access key and secret key.
```bash
aws configure set aws_access_key_id sgiamadmin
aws configure set aws_secret_access_key ldapadmin
```

- Set the endpoint url into variable for the future reference.
```bash
endpoint_url="http://""$(kubectl get svc | grep cortx-server-loadbal | awk '{ print $3 }')"":80"
```

- Configure S3 endpoint in awscli configuration.
```bash
aws configure set plugins.endpoint awscli_plugin_endpoint
aws configure set s3.endpoint_url $endpoint_url
aws configure set s3api.endpoint_url $endpoint_url
```

- Verify configurations.
```bash
cat /root/.aws/config
```

- Create a bucket ad list the bucket.
```bash
aws s3 mb s3://mybucket
aws s3 ls
```

- Create a sample file and copy the object into bucket.
```bash
dd if=/dev/zero of=file.txt bs=10MB count=1
aws s3 cp file.txt s3://mybucket/object
```

- Validate and download the object from the bucket
```bash
aws s3 ls s3://mybucket/
aws s3 cp s3://mybucket/object file-download.txt
```

- Remove the object from the bucket
```bash
aws s3 rm s3://mybucket --recursive
```

- Remove the bucket and validate the deleted bucket.
```bash
aws s3 rb s3://mybucket
aws s3 ls
```
