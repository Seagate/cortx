CORTX S3 Sample Code - curl
==============================

Prerequisites
---------------------
* curl
* openssl

Full Script
---------------------
Sample script name - [cortx-s3samplecode-curl.sh](cortx-s3samplecode-curl.sh)

Session  Setup
---------------------
```sh
s3Server="<yourS3host>"
s3Bucket="<yourbucket>"
s3File="<yourfile>"
s3Key="<yourS3key>"
s3Secret="<yourS3secret>"
```

Create Bucket
---------------------
```sh
(TODO)
```

List Buckets
---------------------
```sh
(TODO)
```

Put Object
---------------------
```sh
(TODO)
```

Get Object
---------------------
```sh
resource="/${s3Bucket}/${s3File}"
contentType="application/octet-stream"
dateValue="`date -u +%a,\ %e\ %b\ %Y\ %T\ %Z`"
stringToSign="GET\n\n${contentType}\n${dateValue}\n${resource}"
signature=`echo -en ${stringToSign} | openssl sha1 -hmac ${s3Secret} -binary | base64`

curl -k -H "Host: ${s3Server}" \
     -H "Date: ${dateValue}" \
     -H "Content-Type: ${contentType}" \
     -H "Authorization: AWS ${s3Key}:${signature}" \
     https://${s3Server}/${s3Bucket}/${s3File}
```

List Objects
---------------------
```sh
(TODO)
```

Delete Object
---------------------
```sh
(TODO)
```

Delete Bucket
---------------------
```sh
(TODO)
```