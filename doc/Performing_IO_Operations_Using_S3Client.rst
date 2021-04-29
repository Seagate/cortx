========================================
Performing IO Operations Using S3 Client
========================================



Prerequisites
=============

Verify that the S3 server endpoint is reachable using the following command:

    ::
  
        ping <s3 endpoint or public data IP>

- If yes then go ahead and configure the s3 client. See `Procedure <#Procedure>`__
- If not then recheck the client VM network configuration.  See `Troubleshoot Virtual Network </doc/troubleshoot_virtual_network.rst>`__

Procedure
=========

#. Install and configure the the AWS CLI Client. See `AWS CLI <https://docs.aws.amazon.com/cli/latest/userguide/install-cliv2-linux.html>`__.

    **Note:** You can install the AWS CLI either on the same OVA or you can create a new VM and install it. If you have installed the AWS CLI on the separate VMs, then the following entry must be added in the */etc/hosts* file of the new VM:

    ::

        <<Public Data IP>> s3.seagate.com sts.seagate.com iam.seagate.com sts.cloud.seagate.com   

#. If you have not already created an S3 user, then navigate to you your CORTX GUI, select 'Manage', then select the 'S3 Account' tab, then click on 'Add new account.'

    .. image:: images/add_s3_user.png

#. After adding the new account, make sure to copy the 'Access Key' and the 'Secret Key'. Note that they will also be available in the CSV which is downloaded when you add the new account.

#. Create a bucket on CORTX GUI:
   
    1. Log-in to CORTX GUI  using S3 account credentials at https://<management IP>:28100/#/
    
    2. Go to the **Manage tab > Bucket > Create Bucket**.
    
    3. Enter the Bucket name and click **Create**.

        .. image:: images/Create-Bucket.png

#. Run the following command to use IOs: 

    ::

        ./s3bench -accessKey=<Enter your access key> -accessSecret=<Enter your Secret Key ID> -bucket=<Bucket name> -endpoint=http://<s3 endpoint or Public data IP>-numClients=100 -numSamples=100 -objectNamePrefix=loadgen_test -objectSize=1048576 -region=igneous-test -skipCleanup=False -verbose=True

#. Configure AWS credentials using following commands:
    
    1. Run the following command to configure the AWS configure: 
    
        ::
           
            aws configure

        .. image::  images/aws-configure.png

    1. Set the endpoint URL:
        
        ::

            aws configure set s3.endpoint_url https://s3.seagate.com

    1. Set the API endpoint URL 
            
        ::
        
            aws configure set s3api.endpoint_url https://s3.seagate.com

    1. Set the AWS certificate path:

        ::
        
            aws configure set default.ca_bundle /etc/ssl/stx-s3-clients/s3/ca.crt

    1. Copy the S3 certificate from ova-server to client location:
    
        ::
        
            scp root@<ova-server-ip>:/opt/seagate/cortx/provisioner/srv/components/s3clients/files/ca.crt  /etc/ssl/stx-s3-clients/s3/ca.crt

#. Perform IO operation.

    1. Verify buckect created using CORTX GUI.

        ::
        
            aws s3 ls --endpoint-url=http://s3.seagate.com

        .. image::  images/verify-bkt.png

    2. Run the following command to create a new large file to upload:

        ::
        
            dd if=/dev/zero of=/tmp/1G bs=1G count=1

        .. image::  images/create-file.png

    3. Upload the file in the created bucket:

        ::
        
            aws s3 cp /tmp/1G s3://ova-bucket --endpoint-url https://s3.seagate.com

        .. image::  images/upload.png

    4. Downlonad same file from the bucket:

        ::
        
            aws s3api get-object --bucket ova-bucket --key 1G /tmp/read-1G
    
        .. image::  images/aws-download.png

#. Navigate to the Dashboard tab in your CORTX GUI, change 'Metric 1' to 'throughput_write' and you should see activity in the dashboard as so:
   
    .. image:: images/PG.PNG