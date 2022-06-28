import { BucketCard } from './BucketCard'
import { v4 as uuid } from 'uuid'
import { BezierSpinner } from '../Spinner/BezierSpinner'
import { Button, Image, useColorModeValue } from '@chakra-ui/react'
import { useSelector } from 'react-redux'

export function CortxBuckets({}) {
  const ipfsStore = useSelector((state) => state.ipfsRedux)
  const logoPath = useColorModeValue('/CORTX-Logo-BLK.png', '/CORTX-Logo-WHT.png')
  const buckets = ['ipfs', 'imaginary', 'planetary']

  return (
    <div className="max-w-full sm:max-w-sm items-center">
      <div className="flex flex-shrink items-center text-white mx-auto sm:mx-5 opacity-70 sm:my-6 ">
        {/* <Image width='11' src={logoPath} alt='cortxLogo' */}
        <Image src={'cortx_image.png'} alt="cortxLogo" className="sm:absolute w-40 sm:w-fit" />
      </div>
      <div className={ipfsStore.selectedIdx.length === 0 ? 'invisible' : 'visible'}>
        <div
          className="w-4/5 sm:w-fit flex flex-col bg-gradient-to-b from-slate-900 border-t border-b border-lime-400 text-snow px-6 py-3 mb-6 sm:mx-auto"
          role="alert"
        >
          <p className="font-bold">Available Buckets</p>
          <p className="text-sm ">Select a bucket to upload your IPFS files.</p>
        </div>
        <div className="flex sm:flex-col overflow-x-scroll z-50 scrollbar-hide">
          {buckets.map((bucket, i) => {
            return <BucketCard bucket={bucket} idx={1} key={uuid()} />
          })}
        </div>
      </div>
    </div>
  )
}
