CORTX OVA: Performing S3 IO Operations
======================================


## Prerequisites

1. Necessary packages are already installed.
1. Import OVA on hypervisor i.e. VMware Workstation and wait for 5 to 10 mins during the booting stage till the login screen comes.


## Procedure

- Run the following commands:
```bash
mkdir -p /var/log/cortx/auth
touch /var/log/cortx/auth/s3iamcli.log
kubectl get pods -o wide | grep cortx-data-pod
```

- Edit */etc/hosts* file and copy the IP of service data pod i.e. `cortx-data-pod` on same OVA image
and add following entry if it does not exist.
```bash
<IP_Address_of_cortx-data-pod> s3.seagate.com iam.seagate.com
```

- Create S3 account by running following command:
```bash
s3iamcli CreateAccount -n C_QA -e cloud@seagate.com --ldapuser sgiamadmin --ldappasswd ldapadmin --no-ss
```

The above command will return the keys as per following example:

> [root@cortx-k8s-setup ~]# s3iamcli CreateAccount -n C_QA -e cloud@seagate.com --ldapuser sgiamadmin --ldappasswd ldapadmin --no-ss
AccountId = 600297343713, CanonicalId = dfccc0c3b9154f169408514e32dc20f932c968ed2b6f468bab7aff4cafbe5be8, RootUserName = root, <xxxxAccessKeyxxxx>, SecretKey = <xxxxSecretKeyxxxx>

**Note:** The above command will prompt for Access key and Secret key which can be referenced while configuring `aws configure` 

- Run the following command to configure `awscli`:
```bash
aws configure
```

- Configure S3 endpoint for awscli as follows:
```bash
aws configure set plugins.endpoint awscli_plugin_endpoint
aws configure set s3.endpoint_url http://s3.seagate.com
aws configure set s3api.endpoint_url http://s3.seagate.com
```

- You can verify configurations as follows:
```bash
cat /root/.aws/config
```

- Create a S3 bucket by running:
```bash
aws s3 mb s3://mybucket
```

- Run the following command to list the bucket:
```bash
aws s3 ls
```

- Run the following command to create a new file to upload:
```bash
dd if=/dev/zero of=file10MB bs=1M count=10
```

- Upload the file in the created bucket:
```bash
aws s3 cp file10MB s3://mybucket/file10MB
```

- List the files from bucket:
```bash
aws s3 ls s3://mybucket/
```

- Download the same file from the bucket:
```bash
aws s3 cp s3://mybucket/file10MB file10MBDn
```

**Note:** For more information on CORTX supported S3 APIs, see [S3 Supported API](https://github.com/Seagate/cortx-s3server/blob/main/docs/s3-supported-api.md "S3 Supported API").
