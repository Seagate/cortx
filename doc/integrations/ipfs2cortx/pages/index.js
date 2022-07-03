// import IpfsComponent from "../components/Ipfs/ipfs";

import HomeWrapper from '../components/layout'
import { useEffect, useState } from 'react'
import { IpfsBox } from '../components/Ipfs/IpfsBox'
import { S3Box } from '../components/Cortx/S3box'
// import { S3React } from '../components/Cortx/S3React'
import IpfsInput from '../components/Ipfs/IpfsInput'
import IpfsLs from '../components/Ipfs/IpfsLs'

import { useSelector, useDispatch } from 'react-redux'
import useMyToast from '../hooks/useMyToast'
import { createS3, listBucket } from '../lib/s3Util'
import { CortxBuckets } from '../components/Cortx/CortxBuckets'
import { BezierSpinner } from '../components/Spinner/BezierSpinner'
import { selectBucket, reset } from '../app/cortxSlice'

export default function Home({ buckets }) {
  const store = useSelector((state) => state.ipfsRedux)
  const dispatch = useDispatch()
  const cortxStore = useSelector((state) => state.cortx)
  const toast = useMyToast()

  useEffect(() => {
    if (cortxStore.selectedBucket) {
      setTimeout(() => dispatch(reset()), 50_000)
    }
  })

  return (
    <>
      {cortxStore.selectedBucket && (
        <div className="w-full h-full fixed top-0 left-0 bg-snow opacity-80 z-50">
          <div className="top-1/2 my-0 mx-auto block relative">
            {/* <p className="relative text-aqua text-xs font-bold ml-auto">UPLOADING...</p> */}
            <div className="scale-300 transform-gpu">
              <BezierSpinner />
            </div>
          </div>
        </div>
      )}
      <HomeWrapper>
        <div className="relative sm:flex space-x-11 ">
          <IpfsBox>
            <IpfsInput />
            <IpfsLs />
          </IpfsBox>
          <S3Box>
            <CortxBuckets />
          </S3Box>
        </div>
      </HomeWrapper>
    </>
  )
}

// process.env.NODE_TLS_REJECT_UNAUTHORIZED = 0

// export async function getServerSideProps({ req, res }) {
//   // console.log("🚀 ~ file: index.js ~ line 35 ~ getServerSideProps ~ res", res)
//   // console.log("🚀 ~ file: index.js ~ line 35 ~ getServerSideProps ~ req", req)

//   const s3 = await createS3()
//   // console.log("🚀 ~ file: index.js ~ line 39 ~ getServerSideProps ~ s3", s3)
//   const buckets = await listBucket(s3)
//   console.log("🚀 ~ file: index.js ~ line 40 ~ getServerSideProps ~ buckets", buckets)

//   if (!buckets) {
//     return {
//       notFound: true,
//     }
//   }

//   return { props: { buckets } }
// }
