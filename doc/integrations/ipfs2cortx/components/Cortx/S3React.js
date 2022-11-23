import { Button } from '@chakra-ui/react'
import { listBucket, listObjects } from '../../lib/s3Util'

export function S3React() {
  function handleClick(event) {
    listObjects('ipfsbucket')
  }

  return (
    <div className="mr-0">
      <Button onClick={handleClick}>ListAWS</Button>
    </div>
  )
}
