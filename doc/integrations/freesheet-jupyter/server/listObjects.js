import { createBucket, listObjects, S3 } from "./cortx.js";

// Must ignore cert errors due to Cortx certs
process.env.NODE_TLS_REJECT_UNAUTHORIZED = 0;

const BUCKET_NAME = "freesheet" // Customize name

listObjects(BUCKET_NAME)
