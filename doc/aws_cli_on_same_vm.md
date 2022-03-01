AWS CLI on the same VM
=======================

You can install the AWS CLI on the CORTX OVA to perform IO operations.


1. Ensure that all the prerequisites are satisfied. See [Prerequisites](https://github.com/Seagate/cortx/blob/main/doc/IO_prereqs.md)

1. Install and configure the AWS CLI Client. See [AWS CLI](https://docs.aws.amazon.com/cli/latest/userguide/install-cliv2-linux.html)

1. To create an S3 user account:

    1. On the CORTX GUI, go to the **Manage > S3 Account > Add New Account**.

        ![Alt Text](https://github.com/Seagate/cortx/blob/main/doc/images/add_s3_user.png)

    2. Enter the required details and click **Create**

1. After adding the new account, copy the **Access Key** and **Secret Key**. A CSV file will be downloaded after you create the new account.

1. Create a bucket on CORTX GUI:

    1. Log-in to CORTX GUI  using S3 account credentials at ``https://<management IP>/#/``

    2. Go to the **Manage tab > Bucket > Create Bucket**.

    3. Enter the Bucket name and click **Create**.

         ![Alt text](https://github.com/Seagate/cortx/blob/main/doc/images/Create-Bucket.png)

1. Configure AWS credentials using the following commands:

    1. Run the following command to configure the AWS configure:

            aws configure

        ![Alt Text](https://github.com/Seagate/cortx/blob/main/doc/images/aws-configure.png)

    2. Set the endpoint URL:

            aws configure set s3.endpoint_url https://s3.seagate.com

    3. Set the API endpoint URL:

            aws configure set s3api.endpoint_url https://s3.seagate.com

    4. Set the AWS certificate path:

            aws configure set default.ca_bundle /opt/seagate/cortx/provisioner/srv/components/s3clients/files/ca.crt

1. Perform the IO operation:

    1. Verify the bucket created using CORTX GUI:

            aws s3 ls --endpoint-url=http://s3.seagate.com

        ![Alt Text](https://github.com/Seagate/cortx/blob/main/doc/images/verify-bkt.png)

    2. Run the following command to create a new large file to upload:

            dd if=/dev/zero of=/tmp/1G bs=1G count=1

        [![Alt Text](https://github.com/Seagate/cortx/blob/main/doc/images/create-file.png)

    3. Upload the file in the created bucket:

        ::

            aws s3 cp /tmp/1G s3://ova-bucket --endpoint-url https://s3.seagate.com

        ![Alt Text](https://github.com/Seagate/cortx/blob/main/doc/images/upload.png)

    4. Download the same file from the bucket:

            aws s3api get-object --bucket ova-bucket --key 1G /tmp/read-1G --endpoint-url https://s3.seagate.com

        ![Alt Text](https://github.com/Seagate/cortx/blob/main/doc/images/aws-download.png)

        **Note:** For more information on CORTX supported S3 APIs, see `S3 Supported API <https://github.com/Seagate/cortx-s3server/blob/main/docs/s3-supported-api.md>`__.

1. Navigate to the Dashboard tab in your CORTX GUI, change *Metric 1* to *throughput_write* and you should see activity in the dashboard.

    ![Alt Text](https://github.com/Seagate/cortx/blob/main/doc/images/PG.PNG)
