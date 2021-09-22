# Build & Deploy CORTX Stack on Amazon Web Services 

This file consists of the procedure to compile complete CORTX stack and Deploy on AWS instance


## Prerequisite 
#### Please ensure you have following available with you 

 -  [x]  AWS Account with Secret Key and Access Key

## Install and setup Terraform and AWS CLI

   Clone cortx-re repository and change directory to cortx-re/solutions/community-deploy/cloud/AWS
```
    git clone https://github.com/Seagate/cortx-re 
    cd $PWD/cortx-re/solutions/community-deploy/cloud/AWS
```

   Install required tools by executing `tool_setup.sh`
```
   ./tool_setup.sh
```

   It will prompt you for AWS Access and Secret key. For details refer [AWS CLI Configuration](https://docs.aws.amazon.com/cli/latest/userguide/cli-configure-quickstart.html#cli-configure-quickstart-config)

   Modify user.tfvars file with your AWS Details.
```
   os_version          = "<OS VERSION>"
   region              = "<AWS REGION>"
   security_group_cidr = "<YOUR PUBLIC IP CIDR>"
```
   Use `CentOS 7.8.2003 x86_64` or `CentOS 7.9.2009 x86_64` as os_version as required.

   If you are not aware of Public IP . Use `curl ipinfo.io/ip `  to identify your Public IP
   It should show output as,
```
   [root@cortx-test AWS]# curl ipinfo.io/ip
   134.204.222.36[root@cortx-test AWS]#
```
   Calculate CIDR for IP using Subnet Calculator from https://mxtoolbox.com/subnetcalculator.aspx 

   Content of `user.tfvars` file should look like as below,
```
   [root@cortx-test AWS]# cat user.tfvars
   os_version          = "CentOS 7.8.2003 x86_64"
   region              = "ap-south-1"
   security_group_cidr = "134.204.222.36/32"
```

## Create AWS instance

   Execute Terraform code to create AWS instance for CORTX Build and Deployment. Command will print public-ip on completion. We will use it to connect AWS instance using SSH Protocol. 
```
   terraform validate && terraform apply -var-file user.tfvars --auto-approve
```
## Network and Storage Configuration.

   Connect to system using SSH key and centos as user.

```
   ssh -i cortx.pem centos@"<AWS instance public-ip>" 
```

   Execute `/home/centos/setup.sh` to setup Network and Storage devices for CORTX. Script will reboot instance on completion. 
```
   sudo bash /home/centos/setup.sh
```
   AWS instance is ready for CORTX Build and deployment now. Connect to instance over SSH and validate that all three network card's has IP address assigned.
   
   Generate `root` user password. It will be required as part of CORTX deployment
   
```
   passwd root
```   

## CORTX Build and Deployment

### 1. CORTX Build

   We will use [cortx-build](https://github.com/Seagate/cortx/pkgs/container/cortx-build) docker image to compile entire CORTX stack. Please follow [CORTX compilation](https://github.com/Seagate/cortx/blob/main/doc/community-build/Generate-Cortx-Build-Stack.md) steps for compilation.

### 2. CORTX Deployment

   After CORTX build is ready follow [CORTX Deployment](https://github.com/Seagate/cortx/blob/main/doc/community-build/ProvisionReleaseBuild.md) to deploy CORTX on AWS instance. Please exclude SELINUX and Hostname setup steps.

### 3. OnBoarding 

   Configure CORTX by following [Onboarding](https://github.com/Seagate/cortx/blob/main/doc/Preboarding_and_Onboarding.rst) steps.

### 4. Testing IO operations 

   We will need separate AWS instance to test IO operations. Follow [IO Test using AWS ](https://github.com/Seagate/cortx/blob/main/doc/integrations/AWS_EC2.md#step-7-create-another-ec2-instance-to-access-to-act-as-the-s3-client) and subsequent steps. 

## Cleanup 

   You can clean-up all AWS infrastructure created using below command. 
```
   terraform validate && terraform destroy -var-file user.tfvars --auto-approve
```
