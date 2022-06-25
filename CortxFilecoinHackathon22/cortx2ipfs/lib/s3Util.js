import * as AWS from 'aws-sdk'
import { Endpoint } from 'aws-sdk';


const S3 = new AWS.S3(
    {
        // region: "eu-central-1",
        credentials: new AWS.Credentials("sgiamadmin", "ldapadmin"),
        endpoint: "http://uvo1d3803qi367n3fq6.vm.cld.sr:31949",
        s3ForcePathStyle: true,
    }
);


export function createS3() {
    const S3 = new AWS.S3(
        {
            region: "eu-central-1",
            credentials: new AWS.Credentials("sgiamadmin", "ldapadmin"),
            endpoint: "http://uvo1d3803qi367n3fq6.vm.cld.sr:31949",
            s3ForcePathStyle: true,
        }
    )
    return S3
}

export async function createBucket(S3, bucketname, CreateBucketConfiguration) {
    // Create a bucket
    console.log("Creating a bucket...");
    let createBucketResults = await S3.createBucket({
        Bucket: bucketname,
        CreateBucketConfiguration,
    }).promise();
    console.dir(createBucketResults);
}

export async function listBucket() {
    // List all buckets.
    console.log("Listing buckets...");
    console.log(S3.endpoint);
    let listBucketsResults = await S3.listBuckets().promise();
    console.dir(listBucketsResults);
}

export async function deployObject(S3, objName, objData, bucketName) {
    // Deploy object to S3 bucket.
    let putObjectResults = await S3.putObject({ Bucket: bucketName, Key: objName, Body: objData }).promise();
    console.dir(putObjectResults);
}

export async function getObject(S3, objName, bucketName) {
    // Get an object
    console.log("Getting an object...");
    let getObjectResults = await S3.getObject({ Bucket: bucketName, Key: objName }).promise();
    console.dir(getObjectResults);
}

export async function listObjects(bucketName) {
    // List objects
    console.log("Listing objects from bucket " + bucketName);
    let listObjectsResults = await S3.listObjects({ Bucket: bucketName }).promise();
    console.dir(listObjectsResults);
}
