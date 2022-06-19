import env from "../enviroment/env";
import CORTX from "cortx-js-sdk";


/* original code */

// AWS.config.update({
//     accessKeyId: env.s3ID,
//     secretAccessKey: env.s3Key
// });
//
// const s3 = new AWS.S3();

// export default s3;
// module.exports = s3;


/* cortx-js-sdk integration */
const {s3Endpoint = "", s3ID = "", s3Key = "", s3Bucket = ""} = env;
const cortx = new CORTX(s3Endpoint, s3ID, s3Key);
const s3 = cortx.createS3Connector();


console.log("loading Cortx....");
s3.listObjects({Bucket: s3Bucket})
    .promise()
    .then((data) => {
        console.log("S3 Cortex is successfully connected")
    })
    .catch(e => {
        console.error("Error connecting to CORTX-S3", e)
    })

export default s3;
