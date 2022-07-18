// S3 call to list Buckets

import { createS3, listBucket } from '../../lib/s3Util'

export default async function handler(req, res) {
  console.log('Getting Bucktes...')
  const s3 = await createS3()
  try {
    let buckets = await listBucket(s3)
    console.log('🚀 ~ file: s3listBuckets.js ~ line 11 ~ handler ~ buckets', buckets)
    res.status(200).json({ buckets })
  } catch (e) {
    res.status(500).json({ error: e })
  }
}
