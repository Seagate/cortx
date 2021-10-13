CyberDuck for IO Operations
============================


You can install the CyberDuck on the CORTX OVA to perform IO operations.


1. Ensure that all the prerequisites are satisfied. See [Prerequisites](https://github.com/Seagate/cortx/blob/hessio-patch-7/doc/IO_prereqs.md) 

1. If you do not already have one, download an S3 client. These instructions use [CyberDuck](https://cyberduck.io/download/)

1. Create an S3 user, on the CORTX GUI navigate to **Manage > S3 Account** tab, then click on **Add New Account**.

   ![Alt text](https://github.com/Seagate/cortx/blob/main/doc/images/add_s3_user.png)

1. After adding the new account, copy the **Access Key** and **Secret Key**. A CSV file will be downloaded after you create the new account.

1. On your S3 client, open a connection:
    
    1. Select the protocol as Amazon S3.
    1. In the Server field, enter the data IP determined while configuring the CORTX.
    1. Enter the access key.
    1. Enter the secret key. 

        ![Alt text](https://github.com/Seagate/cortx/blob/main/doc/images/open_cyberduck_connection.png)

    1. If you see a Certificate error when you open the connection, you can ignore this error.  To suppress this error, you can add an SSL certificate in the Settings tab of the CORTX GUI.

1. After opening the connection in CyberDuck, create a new folder and then upload a large file into that folder.

1. Navigate to the Dashboard tab in the CORTX GUI, change *Metric 1* to *throughput_write* and you should see activity in the dashboard as so:

   ![Alt text](https://github.com/Seagate/cortx/blob/main/doc/images/PG.PNG)
