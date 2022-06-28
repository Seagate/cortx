Prerequisites
=============

- Verify that the S3 server endpoint is reachable using the following command:

            ping <s3 endpoint or public data IP>

    - If yes, then go ahead and configure the s3 client.
    - If not, then recheck the client VM network configuration.  See [Troubleshoot Virtual Network](https://github.com/Seagate/cortx/blob/main/doc/troubleshoot_virtual_network.rst)

- If you are using AWS CLI for operation then install the unzip package:

        yum install unzip -y
