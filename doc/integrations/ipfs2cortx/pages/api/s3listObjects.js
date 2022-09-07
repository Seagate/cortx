// Get all objects inside a bucket

import { createS3, listBucket, listObjects } from '../../lib/s3Util'

export default async function handler(req, res) {
  const bucket = req.query.bucket
  console.log('Reqq', req.query)
  const s3 = await createS3()
  try {
    let objects = await listObjects(s3, bucket)
    res.status(200).json({ objects })
  } catch (e) {
    res.status(500).json({ error: e })
  }
}
