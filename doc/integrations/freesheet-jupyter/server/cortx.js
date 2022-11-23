import AWS from 'aws-sdk'

// Must ignore cert errors due to Cortx certs
process.env.NODE_TLS_REJECT_UNAUTHORIZED = 0;

const PORT = 31949

let s3Endpoint = `http://${process.env.FS_S3_URL}:${PORT}`
let s3AccessKeyID = process.env.FS_ACCESS_KEY;
let s3SecretAccessKey = process.env.FS_SECRET_KEY;

// const REGION = ['us-east-1', 'us-east-2','us-west-1','us-west-2',]

const  endpoint = new AWS.Endpoint(s3Endpoint);
endpoint.port = PORT
// endpoint.protocol = 'http'
const credentials = new AWS.Credentials(s3AccessKeyID, s3SecretAccessKey);
// const region = 'us-east-1' // "us-east-1";
export const S3 = new AWS.S3({endpoint, credentials, s3ForcePathStyle: true, apiVersion: '2006-03-01'});
// S3.config.region = undefined
console.log('init server', S3.config.region, s3Endpoint, s3AccessKeyID, s3SecretAccessKey);

// Example data.
// let testBucketName = "testbucket"; // Bucket
// let testObjectName = "testobject.txt"; // Key
// let testObjectData = "...some random data..."; // Body

const FILE_BASE = `files/`

const getKey = key => `${FILE_BASE}${key}`

export const getBuckets = async ()  => {
  console.log("Listing buckets...");
  return await S3.listBuckets().promise();
}

// List objects
export const listObjects = async (Bucket) => {
  console.log("Listing objects...");
  let listObjectsResults = await S3.listObjects({ Bucket, Prefix: FILE_BASE }).promise();
  console.log('results', listObjectsResults);
  return listObjectsResults;
};

export const getObject = async (Bucket, key) => {
  // Get an object
  console.log("Getting an object...");
  const Key = getKey(key)
  let getObjectResults = await S3.getObject({ Bucket, Key }).promise();
  console.log('getObject', getObjectResults);
  return getObjectResults;
};

export const putObject = async (Bucket, key, Body) => {
  // Put an object
  const Key = getKey(key)
  console.log("Putting an object...", Bucket, Key, Body.length);
  let putObjectResults = await S3.putObject({ Bucket, Key, Body }).promise();
  console.dir(putObjectResults);
  return putObjectResults;
};

// Create a bucket
export const createBucket = async (Bucket) => {
  console.log("Creating a bucket:" + BUCKET_NAME);
  const bucketRequest = S3.createBucket({ Bucket, CreateBucketConfiguration: { LocationConstraint: ""}});
  let createBucketResults = await bucketRequest.promise();
  console.log("created bucket: " + JSON.stringify(createBucketResults));
  return createBucketResults
}

export const deleteObject = async (Bucket, key) => {
  // Delete an object
  console.log("Deleting an object...");
  const Key = getKey(key)
  let deleteObjectResults = await S3.deleteObject({ Bucket, Key }).promise();
  console.dir(deleteObjectResults);
  return deleteObjectResults;
};
