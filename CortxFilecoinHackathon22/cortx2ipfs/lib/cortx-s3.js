"use strict";
const AWS = require("aws-sdk");

// Must ignore cert errors due to Cortx certs
process.env.NODE_TLS_REJECT_UNAUTHORIZED = 0;

const S3 = new AWS.S3(
    {
        region: "eu-central-1",
        credentials: new AWS.Credentials("sgiamadmin", "ldapadmin"),
        endpoint: "http://uvo1d3803qi367n3fq6.vm.cld.sr:31949",
        s3ForcePathStyle: true,
    }
);

let testBucketName = "bariiii"  //"testbucket";
let testObjectName = "testobject.txt";
let testObjectData = "...some random data...";

let sampleScript = async () => {

    // Create a bucket
    console.log("Creating a bucket...");
    let CreateBucketConfiguration = {
        LocationConstraint: ""
    };

    try {
        let createBucketResults = await S3.createBucket({
            Bucket: testBucketName,
            CreateBucketConfiguration,
        }).promise()
        console.dir(createBucketResults)
    } catch (e) { console.log(e) }
    // List buckets

    console.log("Listing buckets...");
    let listBucketsResults = await S3.listBuckets().promise();
    console.dir(listBucketsResults);

    // Put an object
    console.log("Putting an object...");
    let putObjectResults = await S3.putObject({ Bucket: testBucketName, Key: testObjectName, Body: testObjectData }).promise();
    console.dir(putObjectResults);

    // Get an object
    console.log("Getting an object...");
    let getObjectResults = await S3.getObject({ Bucket: testBucketName, Key: testObjectName }).promise();
    console.dir(getObjectResults);

    // List objects
    console.log("Listing objects...");
    let listObjectsResults = await S3.listObjects({ Bucket: testBucketName }).promise();
    console.dir(listObjectsResults);

    // Delete an object
    console.log("Deleting an object...");
    let deleteObjectResults = await S3.deleteObject({ Bucket: testBucketName, Key: testObjectName }).promise();
    console.dir(deleteObjectResults);

    // Delete a bucket
    console.log("Deleting a bucket...");
    let deleteBucketResults = await S3.deleteBucket({ Bucket: testBucketName }).promise();
    console.dir(deleteBucketResults);
}

sampleScript();
