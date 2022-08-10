CORTX S3 Sample Code - node.js
==============================

Prerequisites
---------------------
* npm, node.js 14.18.1 installation for CentOS 7.9
```
$ curl –sL https://rpm.nodesource.com/setup_14.x | sudo bash -
$ sudo yum install –y nodejs
```
* aws-sdk installation
```
$ npm install aws-sdk
```

Full Script
---------------------
Sample script name - [cortx-s3samplecode-node.js](cortx-s3samplecode-node.js)

Run Script
---------------------
```
$ node cortx-s3samplecode-node.js
```

Session  Setup
---------------------
```javascript
const AWS = require('aws-sdk');

// Must ignore cert errors due to Cortx certs
process.env.NODE_TLS_REJECT_UNAUTHORIZED = 0;

let s3Endpoint = "https://<s3server>:443";
let s3AccessKeyID = "<s3accesskey>";
let s3SecretAccessKey = "<s3secretkey>";

let S3 = new AWS.S3();
S3.config.s3ForcePathStyle = true;
S3.config.credentials = new AWS.Credentials(s3AccessKeyID, s3SecretAccessKey);
S3.endpoint = s3Endpoint;

let testBucketName = "testbucket";
let testObjectName = "testobject.txt";
let testObjectData = "...some random data...";
```

Create Bucket
---------------------
```javascript
    // Create a bucket
    console.log("Creating a bucket...");
    let createBucketResults = await S3.createBucket({ Bucket: testBucketName }).promise();
    console.dir(createBucketResults);
```

List Buckets
---------------------
```javascript
    // List buckets
    console.log("Listing buckets...");
    let listBucketsResults = await S3.listBuckets().promise();
    console.dir(listBucketsResults);
```

Put Object
---------------------
```javascript
    // Put an object
    console.log("Putting an object...");
    let putObjectResults = await S3.putObject({ Bucket: testBucketName, Key: testObjectName, Body: testObjectData }).promise();
    console.dir(putObjectResults);
```

Get Object
---------------------
```javascript
    // Get an object
    console.log("Getting an object...");
    let getObjectResults = await S3.getObject({ Bucket: testBucketName, Key: testObjectName }).promise();
    console.dir(getObjectResults);
```

List Objects
---------------------
```javascript
    // List objects
    console.log("Listing objects...");
    let listObjectsResults = await S3.listObjects({ Bucket: testBucketName }).promise();
    console.dir(listObjectsResults);
```

Delete Object
---------------------
```javascript
    // Delete an object
    console.log("Deleting an object...");
    let deleteObjectResults = await S3.deleteObject({ Bucket: testBucketName, Key: testObjectName }).promise();
    console.dir(deleteObjectResults);
```

Delete Bucket
---------------------
```javascript
    // Delete a bucket
    console.log("Deleting a bucket...");
    let deleteBucketResults = await S3.deleteBucket({ Bucket: testBucketName }).promise();
    console.dir(deleteBucketResults);
```

### Tested By:
* November 28, 2021: Bo Wei (bo.b.wei@seagate.com) using Node.js 14.18.1, CentOS 7.9.
* August 2, 2021: Pete Brown (jpbrown@adhdtech.com) using Node.js 14.15.1
