AWS CLI on the same VM
=======================

You can install the AWS CLI on the CORTX OVA to perform IO operations.


### Prerequisites

1. RPM Packages, cortx-s3iamcli & cortx-s3iamcli-devel are installed.
1. Python packages, awscli and awscli-plugin-endpoint are installed.
1. Endpoints are configured via awscli configuration i.e. aws configure & can be verified by running, `cat /root/.aws/config`
1. Import OVA on hypervisor i.e. VMware Workstation and wait for 5 to 10 mins during the booting stage till the login screen comes.


### Procedure

1. Run the following command:

```bash
kubectl get pods -o wide | grep cortx-data-pod
```

1. Edit /etc/hosts file and copy the IP of service data pod i.e. cortx-data-pod-cortx-ova-k8

1. Add the following entry in the */etc/hosts* file:

```bash
<IP_Address> s3.seagate.com iam.seagate.com
```

1. Verify configurations as follows:

```bash
cat /root/.aws/config
```

1. Create S3 account by running following command:

```bash
s3iamcli CreateAccount -n C_QA -e cloud@seagate.com --ldapuser sgiamadmin --ldappasswd ldapadmin --no-ss
```

The above command will return the keys as per following example:

`[root@cortx-k8s-setup ~]# s3iamcli CreateAccount -n C_QA -e cloud@seagate.com --ldapuser sgiamadmin --ldappasswd ldapadmin --no-ss
AccountId = 600297343713, CanonicalId = dfccc0c3b9154f169408514e32dc20f932c968ed2b6f468bab7aff4cafbe5be8, RootUserName = root, <xxxxAccessKeyxxxx>, SecretKey = <xxxxSecretKeyxxxx>`

1. Create a new S3 bucket
```bash
aws s3 mb s3://mybucket
```

1. Run the following command to create a new file to upload:
```bash
dd if=/dev/zero of=file10MB bs=1M count=10
```

1. Upload the file in the created bucket:
```bash
aws s3 cp file10MB s3://mybucket/file10MB
```

1. List the files from bucket:
```bash
aws s3 ls s3://mybucket/
```

1. Download the same file from the bucket:
```bash
aws s3 cp s3://mybucket/file10MB file10MBDn
```

**Note:** For more information on CORTX supported S3 APIs, see `S3 Supported API <https://github.com/Seagate/cortx-s3server/blob/main/docs/s3-supported-api.md>`.
