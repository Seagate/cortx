import { AWS } from 'aws-sdk'


function 
let S3 = new AWS.S3();
S3.config.s3ForcePathStyle = true;
S3.config.credentials = new AWS.Credentials(s3AccessKeyID, s3SecretAccessKey);
S3.endpoint = s3Endpoint;

export function ()