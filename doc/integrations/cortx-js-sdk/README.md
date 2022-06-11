<img src="images/logo.png">
 
# CORTX Javascript/Typescript SDK
 
## Javascript is eating the world. dont stay behind.
<img style="max-width:200px" src="https://media.giphy.com/media/l0MYwONBGDS7aPGOk/source.gif">
 
 
## Motivation
 
Javascript is one of the most used languages in the world, According to [StackOverflow](https://insights.stackoverflow.com/trends?tags=java%2Cc%2Cc%2B%2B%2Cpython%2Cc%23%2Cvb.net%2Cjavascript%2Cassembly%2Cphp%2Cperl%2Cruby%2Cvb%2Cswift%2Cr%2Cobjective-c). 

This makes total sense, as we can run it in the frontend , backend , mobile , wearables , cars or even... fridgeders!
 
 
<img style="max-width:400px" src="images/SO.png">
 
By making CORTX and "Object Storage Technologies"
(which are traditionally ruled by devops or backend engineers at best) available for frontend, mobile and fullstack engineers we open new doors for CORTX and the millions developers in the JS eco-system.
 
## Installation 
 
[![NPM](https://nodei.co/npm/cortx-sdk-js.png)](https://npmjs.org/package/cortx-sdk-js)
 
```sh
npm install --save cortx-js-sdk
```
 
# Connectors
 
### Amazon S3
 
`cortx-js-sdk` exposes a fully wrapped AWS.S3 class to communicate with CORTX
 
```typescript
const cortx = new CORTX(S3_ENDPOINT, S3_ACCESS_KEY_ID, S3_SECRET_ACCESS_KEY);
const s3 = cortx.createS3Connector();
```
 
in this example, the `s3` object is a proxy object to Cortx.
 
```typescript
const res = await s3
  .putObject({
    Bucket: 'testbucket/',
    Key: 'file.txt',
    Body: Buffer.from(file),
    ACL: 'public-read',
  })
  .promise();
```
 
## Examples
 
- any example shown on [Amazon s3 KB](https://docs.aws.amazon.com/sdk-for-javascript/v2/developer-guide/s3-node-examples.html) can be interchangeably used with the `cortx.createS3Connector()` object.
 
- Find more examples on the project github under the `./example/src/` folder

### NodeJS

- Initialize a new npm project and install dependencies (aws-sdk is not automatically installed)
  ```sh
  npm init
  npm i --save cortx-js-sdk
  npm i aws-sdk@2.892.0
  ```
  
- Copy and paste the following code to `index.js` and modify:
  - **S3 Endpoint, Access Key ID, Secret Access Key, File Path**

  ```javascript
  const CORTX = require("cortx-js-sdk");
  const fs = require('fs')

  const S3_ENDPOINT = ''
  const S3_ACCESS_KEY_ID = ''
  const S3_SECRET_ACCESS_KEY = ''

  const filePath = 'C:\\Users\\Downloads\\image.jpg' // Absolute file path
  const bucketName = 'mybucket'

  const cortx = new CORTX.default(S3_ENDPOINT, S3_ACCESS_KEY_ID, S3_SECRET_ACCESS_KEY);
  const s3 = cortx.createS3Connector();

  s3.getBucketLocation({Bucket: bucketName}, function(err, data) {
    if (err) {
      s3.createBucket({ Bucket: bucketName }, function(err, data) {
        if (err) {
          console.log("Create bucket failed", err);
        } else {
          console.log("Bucket created")
          putObject();
        }
      });
    } else {
      console.log("Bucket exists");
      putObject();
    }
  })

  function putObject() {
    var fileStream = fs.createReadStream(filePath);
    fileStream.on('error', function(err) {
      console.log("File stream error", err);
    });
    s3.putObject({
      Bucket: bucketName,
      Key: 'file.txt',
      Body: fileStream,
      ACL: 'public-read',
    }).promise().then(x => {
      console.log("Complete", x)
    });
  }
  ```
- Run the following command to start

  `node index.js`

  Sample Output:
  ```sh
  $ node index.js
  Bucket created
  Complete { ETag: '"c3bef121dd0aa891d5895004542f912f"' }
  ```

### MOTR
 
TBD , once Cortx release MOTR APIs.
 
### Google Cloud Storage
 
TBD , once Cortx integrates with GCS.
 
 
## Can I use it with my Typescript project?
 
`cortx-sdk-js` comes with its own type definitions. battery included ⚡.
 
if you're using "import/export" syntax in your project, you can simply `import CORTX from "cortx-js-sdk";` 
to get the full types definitions.
 
## Can I use it on my frontend?
 
Absolutely! the package is a thin lightweight wrapper which is "bundle-size" friendly ⚖

Tested by:

  - Sep 19, 2021: Harrison Seow (<harrison.seow@seagate.com>) using Cortx OVA 2.0.0 running on VMware Workstation Player 16 on Windows 10.
