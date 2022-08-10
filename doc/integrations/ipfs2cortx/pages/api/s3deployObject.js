// Get all objects inside a bucket

import { createS3, listBucket, deployObject } from '../../lib/s3Util'

export default async function handler(req, res) {
  const { objName, objData, bucketName } = req.body
  const s3 = await createS3()
  try {
    let s3res = await deployObject(s3, objName, objData, bucketName)
    res.status(200).json({ message: s3res })
  } catch (e) {
    res.status(500).json({ error: e })
  }
}

export const config = {
  api: {
    bodyParser: {
      sizeLimit: '11gb',
    },
  },
}
