CORTX Sample S3 Code
==============================

Now that your CORTX instance is up and running, let's look at some sample code and the basic operations used to read and write data via S3.

Basic Operations
---------------------
|Operation| Purpose|
|-|-|
|createBucket|Create a new S3 bucket|
|listBuckets|List S3 buckets available to user|
|deleteBucket|Delete an S3 bucket and contents|
|putObject|Upload an object to a bucket|
|listObjects|List objects in a bucket|
|deleteObject|Delete an object in a bucket|

Script Prerequisites
---------------------
In order to run these scripts, you'll need the CORTX S3 server URL and S3 user credentials.  In a lab setup, the S3 server URL will be **https://***<dataIP>***:443** where *<dataIP>* is the IP address of the public data interface (ens33 in most cases) of the CORTX S3 server.

>Tip: It is strongly recommended that you verify connectivity with Cyberduck to ensure that the target S3 server is reachable and the S3 credentials are valid.  If Cyberduck cannot connect, chances are these scripts won't be able to either.

Sample Scripts
---------------------
Each script will execute a series of operations.  A new bucket will be created, populated with an object, listed then deleted.
-  [curl](curl/)
-  [python](python/)
-  [node.js](node.js/)
-  [go](go/)
-  [template](template/) (template used for sample script README.md)