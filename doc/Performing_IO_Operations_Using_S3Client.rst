========================================
Performing IO Operations Using S3 Client
========================================

This guide provides step-by-step instructions to perform the IO operation using CyberDuck and S3 CLI. 

You need to install the S3 client to perform the IO operations in the CORTX. The AWS CLI can either installed on the same VM or the different VM.

For more information on:

-  To use CyberDuck for IO operations, see `CyberDuck for IO Operations <#CyberDuck-for-IO-Operations>`__.
-  AWS CLI on the same OVA, see `AWS CLI on the CORTX OVA VM <#AWS-CLI-on-the-same-VM>`__.
-  AWS CLI on the new VM, see `AWS CLI on a different VM <#AWS-CLI-on-a-different-VM>`__.

Prerequisites
=============

* Verify that the S3 server endpoint is reachable using the following command:
    .. code-block:: sh

        ping <s3 endpoint or public data IP>
        
    - If yes, then go ahead and configure the s3 client.
    - If not, then recheck the client VM network configuration.  See `Troubleshoot Virtual Network <https://github.com/Seagate/cortx/blob/main/doc/troubleshoot_virtual_network.rst>`__.

* If you are using AWS CLI for operation then install the unzip package:
    .. code-block:: sh

        yum install unzip -y


CyberDuck for IO Operations
============================

You can install the CyberDuck on the CORTX OVA to perform IO operations.

.. raw:: html

    <details>
   <summary><a>Click here to expand the instructions.</a></summary>

#. Ensure that all the prerequisites are satisfied. See `Prerequisites <#Prerequisites>`__.

#. If you do not already have one, download an S3 client. These instructions use `CyberDuck <https://cyberduck.io/download/>`_.

#. Create an S3 user, on the CORTX GUI navigate to **Manage > S3 Account** tab, then click on **Add New Account**.

   .. image:: https://github.com/Seagate/cortx/blob/main/doc/images/add_s3_user.png

#. After adding the new account, copy the **Access Key** and **Secret Key**. A CSV file will be downloaded after you create the new account.

#. On your S3 client, open a connection:
    #. Select the protocol as Amazon S3.
    #. In the Server field, enter the data IP determined while configuring the CORTX.
    #. Enter the access key.
    #. Enter the secret key. 

        .. image:: https://github.com/Seagate/cortx/blob/main/doc/images/open_cyberduck_connection.png

    #. If you see a Certificate error when you open the connection, you can ignore this error.  To suppress this error, you can add an SSL certificate in the Settings tab of the CORTX GUI.

#. After opening the connection in CyberDuck, create a new folder and then upload a large file into that folder.

#. Navigate to the Dashboard tab in the CORTX GUI, change *Metric 1* to *throughput_write* and you should see activity in the dashboard as so:

   .. image:: https://github.com/Seagate/cortx/blob/main/doc/images/PG.PNG

.. raw:: html

    </details>

AWS CLI on the same VM
=======================

You can install the AWS CLI on the CORTX OVA to perform IO operations.

.. raw:: html

    <details>
   <summary><a>Click here to expand the instructions.</a></summary>

#. Ensure that all the prerequisites are satisfied. See `Prerequisites <#Prerequisites>`__.

#. Install and configure the AWS CLI Client. See `AWS CLI <https://docs.aws.amazon.com/cli/latest/userguide/install-cliv2-linux.html>`__.

#. To create an S3 user account:
    #. On the CORTX GUI, go to the **Manage > S3 Account > Add New Account**.

        .. image:: https://github.com/Seagate/cortx/blob/main/doc/images/add_s3_user.png

    #. Enter the required details and click **Create**

#. After adding the new account, copy the **Access Key** and **Secret Key**. A CSV file will be downloaded after you create the new account.

#. Create a bucket on CORTX GUI:
    #. Log-in to CORTX GUI  using S3 account credentials at ``https://<management IP>/#/``

    #. Go to the **Manage tab > Bucket > Create Bucket**.

    #. Enter the Bucket name and click **Create**.

        .. image:: https://github.com/Seagate/cortx/blob/main/doc/images/Create-Bucket.png

#. Configure AWS credentials using the following commands:
    #. Run the following command to configure the AWS configure:
        .. code-block:: sh

            aws configure

        .. image::  https://github.com/Seagate/cortx/blob/main/doc/images/aws-configure.png

    #. Set the endpoint URL:
        .. code-block:: sh

            aws configure set s3.endpoint_url https://s3.seagate.com

    #. Set the API endpoint URL:
        .. code-block:: sh

            aws configure set s3api.endpoint_url https://s3.seagate.com

    #. Set the AWS certificate path:
        .. code-block:: sh

            aws configure set default.ca_bundle /opt/seagate/cortx/provisioner/srv/components/s3clients/files/ca.crt

#. Perform the IO operation:
    #. Verify the bucket created using CORTX GUI:
        .. code-block:: sh

            aws s3 ls --endpoint-url=http://s3.seagate.com

        .. image::  https://github.com/Seagate/cortx/blob/main/doc/images/verify-bkt.png

    #. Run the following command to create a new large file to upload:
        .. code-block:: sh

            dd if=/dev/zero of=/tmp/1G bs=1G count=1

        .. image::  https://github.com/Seagate/cortx/blob/main/doc/images/create-file.png

    #. Upload the file in the created bucket:
        .. code-block:: sh

            aws s3 cp /tmp/1G s3://ova-bucket --endpoint-url https://s3.seagate.com

        .. image::  https://github.com/Seagate/cortx/blob/main/doc/images/upload.png

    #. Download the same file from the bucket:
        .. code-block:: sh

            aws s3api get-object --bucket ova-bucket --key 1G /tmp/read-1G --endpoint-url https://s3.seagate.com

        .. image::  https://github.com/Seagate/cortx/blob/main/doc/images/aws-download.png

        **Note:** For more information on CORTX supported S3 APIs, see `S3 Supported API <https://github.com/Seagate/cortx-s3server/blob/main/docs/s3-supported-api.md>`__.

#. Navigate to the Dashboard tab in your CORTX GUI, change *Metric 1* to *throughput_write* and you should see activity in the dashboard.

    .. image:: https://github.com/Seagate/cortx/blob/main/doc/images/PG.PNG


.. raw:: html

    </details>

AWS CLI on a different VM
===========================

You can also create a different VM and install the AWS CLI on this VM as a client to perform IO operations.

.. raw:: html

    <details>
   <summary><a>Click here to expand the instructions.</a></summary>

#. Ensure that all the prerequisites are satisfied. See `Prerequisites <#Prerequisites>`__.

#. Add the following entry must be added in the */etc/hosts* file of the new VM:
    .. code-block::

        <<Public Data IP>> s3.seagate.com sts.seagate.com iam.seagate.com sts.cloud.seagate.com

#. Install and configure the AWS CLI Client. See `AWS CLI <https://docs.aws.amazon.com/cli/latest/userguide/install-cliv2-linux.html>`__.

#. To create an S3 user account:
     #. On the CORTX GUI, go to the **Manage > S3 Account > Add New Account**.

         .. image:: https://github.com/Seagate/cortx/blob/main/doc/images/add_s3_user.png

     #. Enter the required details and click **Create**

#. After the new account is created, make sure to copy the *Access Key* and the *Secret Key*. The Access Key and Secret Key will also be available in the CSV file which is downloaded when you add the new account.

#. Create a bucket on CORTX GUI:
     #. Log-in to CORTX GUI  using S3 account credentials at ``https://<management IP>:28100/#/``

     #. Go to the **Manage tab > Bucket > Create Bucket**.

     #. Enter the Bucket name and click **Create**.

         .. image:: https://github.com/Seagate/cortx/blob/main/doc/images/Create-Bucket.png

#. Configure the AWS credentials using following commands:
    #. Run the following command to configure the AWS configure:
        .. code-block:: sh

            aws configure

        .. image::  https://github.com/Seagate/cortx/blob/main/doc/images/aws-configure.png

    #. Set the endpoint URL:
        .. code-block:: sh

            aws configure set s3.endpoint_url https://s3.seagate.com

    #. Set the API endpoint URL:
        .. code-block:: sh

            aws configure set s3api.endpoint_url https://s3.seagate.com

    #. Copy the S3 certificate from OVA to the client location:
        .. code-block:: sh

            scp root@<ova-server-ip>:/opt/seagate/cortx/provisioner/srv/components/s3clients/files/ca.crt /etc/ssl/stx-s3-clients/s3/ca.crt

    #. Set the AWS certificate path:
        .. code-block:: sh

            aws configure set default.ca_bundle /etc/ssl/stx-s3-clients/s3/ca.crt

#. Perform IO operation:
    #. Verify bucket created using the CORTX GUI:
        .. code-block:: sh

            aws s3 ls --endpoint-url=http://s3.seagate.com

        .. image::  https://github.com/Seagate/cortx/blob/main/doc/images/verify-bkt.png

    #. Run the following command to create a new large file to upload:
        .. code-block:: sh

            dd if=/dev/zero of=/tmp/1G bs=1G count=1

        .. image::  https://github.com/Seagate/cortx/blob/main/doc/images/create-file.png

    #. Upload the file in the created bucket:
        .. code-block:: sh

            aws s3 cp /tmp/1G s3://ova-bucket --endpoint-url https://s3.seagate.com

        .. image::  https://github.com/Seagate/cortx/blob/main/doc/images/upload.png

    #. Download same file from the bucket:
        .. code-block:: sh

            aws s3api get-object --bucket ova-bucket --key 1G /tmp/read-1G --endpoint-url https://s3.seagate.com

        .. image::  https://github.com/Seagate/cortx/blob/main/doc/images/aws-download.png

        **Note:** For more information on CORTX supported S3 APIs, see `S3 Supported API <https://github.com/Seagate/cortx-s3server/blob/main/docs/s3-supported-api.md>`__.

#. Navigate to the Dashboard tab in your CORTX GUI, change 'Metric 1' to 'throughput_write' and you should see activity in the dashboard.

    .. image:: https://github.com/Seagate/cortx/blob/main/doc/images/PG.PNG



.. raw:: html

    </details>
