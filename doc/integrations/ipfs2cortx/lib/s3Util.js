import * as AWS from 'aws-sdk'
import qs from 'qs'

export async function createS3() {
  const S3 = await new AWS.S3({
    // region: 'eu-central-1',
    credentials: new AWS.Credentials(
      process.env.NEXT_ACCESS_KEY,
      process.env.NEXT_SECRET_ACCESS_KEY
    ),
    endpoint: process.env.NEXT_END_POINT + ':' + process.env.NEXT_PORT,
    s3ForcePathStyle: true,
  })
  return S3
}

export async function createBucket(S3, bucketname) {
  // Create a bucket
  console.log('Creating a bucket...')
  let CreateBucketConfiguration = {
    LocationConstraint: '',
  }
  let createBucketResults = await S3.createBucket({
    Bucket: bucketname,
    CreateBucketConfiguration,
  }).promise()
  console.dir(createBucketResults)
}

export async function listBucket(S3) {
  // List all buckets.
  console.log('Listing buckets...')
  let listBucketsResults = await S3.listBuckets().promise()
  console.dir(listBucketsResults)
  return listBucketsResults
}

export async function deployObject(S3, objName, objData, bucketName) {
  console.log(
    '🚀 ~ file: s3Util.js ~ line 42 ~ deployObject ~ objName, bucketName',
    objName,
    bucketName
  )
  // Deploy object to S3 bucket.
  let putObjectResults = await S3.putObject({
    Bucket: bucketName,
    Key: objName,
    Body: qs.stringify(objData),
  }).promise()
  console.dir(putObjectResults)
  return putObjectResults
}

export async function getObject(S3, objName, bucketName) {
  // Get an object
  console.log('Getting an object...')
  let getObjectResults = await S3.getObject({ Bucket: bucketName, Key: objName }).promise()
  console.dir('obj', getObjectResults)
  return getObjectResults
}

export async function listObjects(S3, bucketName) {
  // List objects
  console.log('Listing objects from bucket ' + bucketName)
  let listObjectsResults = await S3.listObjects({ Bucket: bucketName }).promise()
  console.dir(listObjectsResults)
  return listObjectsResults
}
