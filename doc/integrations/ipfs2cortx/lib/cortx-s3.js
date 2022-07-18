'use strict'
const AWS = require('aws-sdk')

// Must ignore cert errors due to Cortx certs
process.env.NODE_TLS_REJECT_UNAUTHORIZED = 0

// To get the port, use this code on the local ova VM:
// kubectl describe svc cortx-server-loadbal-svc-cortx-ova-rgw -n cortx |grep NodePort:

const S3 = new AWS.S3({
  // region: 'eu-central-1',
  credentials: new AWS.Credentials('sgiamadmin', 'ldapadmin'),
  endpoint: 'http://192.168.87.131:30518',
  s3ForcePathStyle: true,
})
// const S3 = new AWS.S3({
//   region: 'eu-central-1',
//   credentials: new AWS.Credentials('sgiamadmin', 'ldapadmin'),
//   endpoint: 'http://uvo1d3803qi367n3fq6.vm.cld.sr:31949',
//   s3ForcePathStyle: true,
// })
// endpoint: 'http://192.168.87.2:31949',
// connect to AWS
// const S3 = new AWS.S3({
//   region: 'eu-central-1',
//   credentials: new AWS.Credentials(process.env.AWS_USER, process.env.AWS_PWD),
//   AWS.S
//   s3ForcePathStyle: true,
// })
// console.log("🚀 ~ file: cortx-s3.js ~ line 18 ~ process.env.AWS_USER", process.env.AWS_USER)

let testBucketName = 'planets' //"testbucket";
let testObjectName = 'pluto.txt'
let testObjectData = '...some random data...'

// Create initial parameters JSON for putBucketCors.
const thisConfig = {
  AllowedHeaders: ['*'],
  AllowedMethods: ['POST', 'GET', 'PUT'],
  AllowedOrigins: ['*'],
  ExposeHeaders: [],
  MaxAgeSeconds: 3000,
}
// Create an array of configs then add the config object to it.
const corsRules = new Array(thisConfig)

// Create CORS parameters.
const corsParams = {
  Bucket: testBucketName,
  CORSConfiguration: { CORSRules: corsRules },
}

let sampleScript = async () => {
  // // Create a bucket
  console.log('Creating a bucket...')
  let CreateBucketConfiguration = {
    LocationConstraint: '',
  }

  try {
    let createBucketResults = await S3.createBucket({
      Bucket: testBucketName,
      CreateBucketConfiguration,
    }).promise()
    console.dir(createBucketResults)
  } catch (e) {
    console.log(e)
  }

  // // Create Bucket CORS
  // console.log('Updating CORS policy ...')
  // try {
  //   const data = await S3.putBucketCors(corsParams)

  //   // send(AWS.S3Control.PutBucketCorsCommand(corsParams));
  //   console.log('Success', data)
  //   return data // For unit tests.
  // } catch (err) {
  //   console.log('Error', err)
  // }

  // List buckets
  console.log('Listing buckets...')
  let listBucketsResults = await S3.listBuckets().promise()
  console.dir(listBucketsResults)

  // Put an object
  console.log('Putting an object...')
  let putObjectResults = await S3.putObject({
    Bucket: testBucketName,
    Key: testObjectName,
    Body: testObjectData,
  }).promise()
  console.dir(putObjectResults)

  // // Get an object
  console.log('Getting an object...')
  let getObjectResults = await S3.getObject({
    Bucket: testBucketName,
    Key: testObjectName,
  }).promise()
  console.dir(getObjectResults)

  // // List objects
  console.log('Listing objects...')
  let listObjectsResults = await S3.listObjects({ Bucket: testBucketName }).promise()
  console.dir(listObjectsResults)

  // Delete an object
  // console.log("Deleting an object...");
  // let deleteObjectResults = await S3.deleteObject({ Bucket: testBucketName, Key: testObjectName }).promise();
  // console.dir(deleteObjectResults);

  // Delete a bucket
  // console.log("Deleting a bucket...");
  // let deleteBucketResults = await S3.deleteBucket({ Bucket: testBucketName }).promise();
  // console.dir(deleteBucketResults);
}

sampleScript()
