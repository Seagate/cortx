#!/bin/sh
s3Server="<yourS3host>"
s3Bucket="<yourbucket>"
s3File="<yourfile>"
s3Key="<yourS3key>"
s3Secret="<yourS3secret>"

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
