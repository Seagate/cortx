AWS CLI on the Windows  
======================

You can also run AWS CLI on Windows machine. Install the AWS CLI for Windows as a client to perform IO operations.

1. Ensure that all the prerequisites are satisfied. See [Prerequisites](https://github.com/Seagate/cortx/blob/main/doc/IO_prereqs.md)

1. Add the following entry must be added in the C:\Windows\System32\drivers\etc\host.txt file of the Windows:

        <<Public Data IP>> s3.seagate.com sts.seagate.com iam.seagate.com sts.cloud.seagate.com

1. Install and configure the AWS CLI Client for Windows. See [AWS CLI](https://docs.aws.amazon.com/cli/latest/userguide/install-cliv2-linux.html)

1. To create an S3 user account:

     1. On the CORTX GUI, go to the **Manage > S3 Account > Add New Account**.

         ![Alt Text](https://github.com/Seagate/cortx/blob/main/doc/images/add_s3_user.png)

     2. Enter the required details and click **Create**

1. After the new account is created, make sure to copy the *Access Key* and the *Secret Key*. The Access Key and Secret Key will also be available in the CSV file which is downloaded when you add the new account.

1. Create a bucket on CORTX GUI:

     1. Log-in to CORTX GUI  using S3 account credentials at ``https://<management IP>:28100/#/``

     2. Go to the **Manage tab > Bucket > Create Bucket**.

     3. Enter the Bucket name and click **Create**.

         ![Alt Text](https://github.com/Seagate/cortx/blob/main/doc/images/Create-Bucket.png)

1. Configure the AWS credentials using following commands:

    1. Run the following command to configure the AWS configure:

            aws configure

        ![Alt Text](/doc/images/wins_aws-configure.png)

    2. Set the endpoint URL:

            aws configure set s3.endpoint_url https://s3.seagate.com

    3. Set the API endpoint URL:

            aws configure set s3api.endpoint_url https://s3.seagate.com

    4. Copy the S3 certificate from OVA to the Windows client location:

            scp root@<ova-server-ip>:/opt/seagate/cortx/provisioner/srv/components/s3clients/files/ca.crt C:\Users\.aws>

    5. Set the AWS certificate path:

            C:\Users\.aws>aws configure set default.ca_bundle c:\Users\.aws\ca.crt

1. Perform IO operation:

    1. Verify bucket created using the CORTX GUI:

            aws s3 ls --endpoint-url=http://s3.seagate.com

        ![Alt Text](/doc/images/wins_verify-bkt.png)

    2. Run the following command to create a new large file size 1GB to upload:

            fsutil file createnew bigfile.out 1000000000

        ![Alt Text](/doc/images/wins_create-file.png)

    3. Upload the file in the created bucket:

            C:\Users\.aws>aws s3 cp bigfile.out s3://ova-bucket --endpoint-url https://s3.seagate.com

        ![Alt Text](/doc/images/wins_upload.png)

    4. Download same file from the bucket:

            C:\Users\.aws>aws s3api get-object --bucket ova-bucket --key bigfile.out .\read-1G --endpoint-url https://s3.seagate.com

        ![Alt Text](/doc/images/wins_aws-download.png)

        **Note:** For more information on CORTX supported S3 APIs, see [S3 Supported API](https://github.com/Seagate/cortx-s3server/blob/main/docs/s3-supported-api.md)

1. Navigate to the Dashboard tab in your CORTX GUI, change 'Metric 1' to 'throughput_write' and you should see activity in the dashboard.

    ![Alt Text](https://github.com/Seagate/cortx/blob/main/doc/images/PG.PNG)
