S3Client Setup
——————————————
Download and extract ‘S3Client Binary.zip’ on your machine. This contains the executables (S3Client.exe, S3Utility.exe and s3smoke.bat) and the required dlls.


Configuration
—————————————
AWS access key id and AWS secret key can be passed to S3Client by
1. Command line arguments - 

   1. Use option “-k” or “--key” to input aws access key id.
   2. Use option “-s” or “--secret” to input aws secret key.

2. Configuration File - 

Create a credentials file - %UserProfile%\.aws\credentials and give the credentials in the following format.
        aws_access_key_id=<AWS ACCESS KEY ID>
        aws_secret_access_key=<AWS SECRET KEY>
        
	Note: Use any random keys for now. Auth is yet to be implemented.
        
	Example: C:\Users\admin\.aws\credentials
        aws_access_key_id=C99F5C7EE00F1EXAMPLE
	aws_secret_access_key=a63xWEj9ZFbigxqA7wI3Nuwj3mte3RDBdEXAMPLE


Configuring etc hosts - Use S3Utility.exe to update etc host file.
      * Extract the S3Client Binary.zip 
      * Open terminal as ADMINISTRATOR.
      * cd to S3Client Binary.
      * run the command - S3Utility.exe -i <IP OF THE SERVER>.
        Use the IP that is generated for the S3 server VM. See s3 server readme.

NOTE - 
____
      * Buckets are not supported by S3 server currently. Hence use only “evault” as  bucket name for testing. 
      * S3 utility will add a dns entry for evault bucket in local file for now.
      * Run S3 Utility only once.




S3 Server Smoke Test
__________________________________

        (This section assumes you have run the etc hosts change described above)
      1. Ensure the C:\Users\admin\.aws\credentials file is created as described above.
      2. ping evault.s3.seagate.com and check if it responds.
      3. Execute s3smoke.bat to run smoke test on the server.

This test uses few input files and performs PUT/GET/DELETE operations for sanity test.


S3 Client Commands
___________________

S3 client (.net sdk) currently supports get, put and delete objects. 
      1. Open Powershell.
      2. cd to directory containing S3Client.exe.
      3. Run S3Client commands


      * Put - S3Client.exe put <path to the file> s3://<bucket name>
	Ex - S3Client.exe put C:\hello.txt s3://mybycket


      * Get - S3Client.exe get s3://<bucket name>/<object>
	Ex - S3Client.exe get s3://mybucket/hello.txt


      * Delete - S3Client del s3://<bucket name>/<object>
	Ex - S3Client.exe del s3://mybucket/hello.txt

      * Help - Use “S3Client.exe --help” flag to check for the correct usage.