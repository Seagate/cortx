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
port="30518"
```

List Buckets
---------------------
```sh
resource="/"
contentType="application/octet-stream"
dateValue="`date -u +%a,\ %e\ %b\ %Y\ %T\ %Z`"
stringToSign="GET\n\n${contentType}\n${dateValue}\n${resource}"
signature=`echo -en ${stringToSign} | openssl sha1 -hmac ${s3Secret} -binary | base64`

curl -k -H "Host: ${s3Server}" \
     -H "Date: ${dateValue}" \
     -H "Content-Type: ${contentType}" \
     -H "Authorization: AWS ${s3Key}:${signature}" \
     http://${s3Server}:${port}/
```

List Objects
---------------------
```sh
resource="/${s3Bucket}/"
contentType="application/octet-stream"
dateValue="`date -u +%a,\ %e\ %b\ %Y\ %T\ %Z`"
stringToSign="GET\n\n${contentType}\n${dateValue}\n${resource}"
signature=`echo -en ${stringToSign} | openssl sha1 -hmac ${s3Secret} -binary | base64`

curl -k -H "Host: ${s3Server}" \
     -H "Date: ${dateValue}" \
     -H "Content-Type: ${contentType}" \
     -H "Authorization: AWS ${s3Key}:${signature}" \
     http://${s3Server}:${port}/${s3Bucket}/
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

Create Bucket
---------------------
```sh
curl -X "PUT" "https://(endpoint)/(bucket-name)/?cors"
 -H "Content-MD5: (md5-hash)"
 -H "Authorization: bearer (token)"
 -H "Content-Type: text/plain; charset=utf-8"
 -d "<CORSConfiguration>
      <CORSRule>
        <AllowedOrigin>(url)</AllowedOrigin>
        <AllowedMethod>(request-type)</AllowedMethod>
        <AllowedHeader>(url)</AllowedHeader>
      </CORSRule>
     </CORSConfiguration>"
```

Put Object
---------------------
```sh
curl -X "PUT" "https://(endpoint)/(bucket-name)/(object-key)" \
 -H "Authorization: bearer (token)" \
 -H "Content-Type: (content-type)" \
 -d "(object-contents)"
```

Delete Object
---------------------
```sh
curl -X "DELETE" "https://(endpoint)/(bucket-name)/(object-key)"
 -H "Authorization: bearer (token)"
```

Delete Bucket
---------------------
```sh
curl -X "DELETE" "https://(endpoint)/(bucket-name)/"
 -H "Authorization: bearer (token)"
```

### Tested By:
* Mar 7, 2022 :
 Digvijay shelar (digvijayshelar@gmail.com)
