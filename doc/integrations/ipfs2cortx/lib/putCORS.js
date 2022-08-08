// Import required AWS-SDK clients and commands for Node.js.
import { PutBucketCorsCommand } from '@aws-sdk/client-s3'
// import { s3Client } from "./libs/s3Client.js"; // Helper function that creates an Amazon S3 service client module.
const AWS = require('aws-sdk')

// Set parameters.
// Create initial parameters JSON for putBucketCors.
const thisConfig = {
  AllowedHeaders: ['Authorization'],
  AllowedMethods: ['POST', 'GET', 'PUT'],
  AllowedOrigins: ['*'],
  ExposeHeaders: [],
  MaxAgeSeconds: 3000,
}

// Assemble the list of allowed methods based on command line parameters
const allowedMethods = []
process.argv.forEach(function (val, index, array) {
  if (val.toUpperCase() === 'POST') {
    allowedMethods.push('POST')
  }
  if (val.toUpperCase() === 'GET') {
    allowedMethods.push('GET')
  }
  if (val.toUpperCase() === 'PUT') {
    allowedMethods.push('PUT')
  }
  if (val.toUpperCase() === 'PATCH') {
    allowedMethods.push('PATCH')
  }
  if (val.toUpperCase() === 'DELETE') {
    allowedMethods.push('DELETE')
  }
  if (val.toUpperCase() === 'HEAD') {
    allowedMethods.push('HEAD')
  }
})

// Copy the array of allowed methods into the config object
thisConfig.AllowedMethods = allowedMethods

// Create an array of configs then add the config object to it.
const corsRules = new Array(thisConfig)

// Create CORS parameters.
const corsParams = {
  Bucket: bucketName,
  CORSConfiguration: { CORSRules: corsRules },
}
async function runCORS(S3) {
  try {
    const data = await S3.send(new AWS.PutBucketCorsCommand(corsParams))
    console.log('Success', data)
    return data // For unit tests.
  } catch (err) {
    console.log('Error', err)
  }
}

run()
