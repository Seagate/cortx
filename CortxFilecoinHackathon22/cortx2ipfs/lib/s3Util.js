import * as AWS from 'aws-sdk'
import { Endpoint } from 'aws-sdk';



// const S3 = new AWS.S3(
//     {
//         region: process.env.NEXT_PUBLIC_REGION,
//         // port: process.env.NEXT_PUBLIC_PORT,
//         credentials: new AWS.Credentials(process.env.NEXT_PUBLIC_ACCESS_KEY, process.env.NEXT_PUBLIC_SECRET_ACCESS_KEY),
//         endpoint: "http://" + process.env.NEXT_PUBLIC_END_POINT + ":" + process.env.NEXT_PUBLIC_PORT,
//         s3ForcePathStyle: true,
//     }
// );

const S3 = new AWS.S3(
    {
        // region: "eu-central-1",
        credentials: new AWS.Credentials("sgiamadmin", "ldapadmin"),
        endpoint: "http://uvo1d3803qi367n3fq6.vm.cld.sr:31949",
        s3ForcePathStyle: true,
    }
);
// S3.config.s3ForcePathStyle = true;
// S3.config.credentials = new AWS.Credentials(process.env.NEXT_PUBLIC_SECRET_ACCESS_KEY, process.env.NEXT_PUBLIC_SECRET_ACCESS_KEY);
// S3.endpoint = process.env.NEXT_PUBLIC_END_POINT;




export async function createBucket(bucketname) {
    // Create a bucket using S3
    // Create a bucket
    console.log("Creating a bucket...");
    let createBucketResults = await S3.createBucket({ Bucket: bucketname }).promise();
    console.dir(createBucketResults);
}

export async function listBucket() {
    // List all buckets.
    console.log("Listing buckets...");
    console.log(S3.endpoint);
    let listBucketsResults = await S3.listBuckets().promise();
    console.dir(listBucketsResults);
}

export async function deployObject(objName, objData, bucketName) {
    // Deploy object to S3 bucket.
    let putObjectResults = await S3.putObject({ Bucket: bucketName, Key: objectName, Body: objectData }).promise();
    console.dir(putObjectResults);
}
