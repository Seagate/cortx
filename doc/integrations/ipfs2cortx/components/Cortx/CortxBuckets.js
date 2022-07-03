import { BucketCard } from './BucketCard'
import { v4 as uuid } from 'uuid'
import { BezierSpinner } from '../Spinner/BezierSpinner'
import { Button, Image, useColorModeValue } from '@chakra-ui/react'
import { useSelector, useDispatch } from 'react-redux'
import { useLazySayHiJonQuery, useGetBucketsQuery } from '../../app/bridgeApi'
import { useEffect } from 'react'
import { listBuckets } from '../../app/cortxSlice'
import { motion, AnimatePresence } from 'framer-motion'
import useMyToast from '../../hooks/useMyToast'


export function CortxBuckets({ }) {
  const store = useSelector((state) => state.cortx)
  const ipfsStore = useSelector((state) => state.ipfsRedux)
  const toast = useMyToast()
  const dispatch = useDispatch()
  const { data, error, isLoading, isError } = useGetBucketsQuery()

  useEffect(() => {
    if (data) {
      const buckets = data?.buckets?.Buckets?.map((bckt) => bckt.Name)
      dispatch(listBuckets({ buckets }))
    } else if (isLoading) {
      // for demo purposes
      setTimeout(() => {
        if (store.buckets?.length > 0) {
          return
        } else {
          const buckets = ['ipfs', 'imaginary', 'planetary']
          dispatch(listBuckets({ buckets }))
          toast('info', `Cluster unavailable! Using mock-up buckets for demonstration purposes.`, 'mockupInfo')
        }
      }, "11000")
    }
  }, [data, isLoading, dispatch, toast, store])

  return (
    <div className="max-w-full sm:max-w-sm items-center">
      <div className="flex flex-shrink items-center text-white mx-auto sm:mx-5 opacity-70 sm:my-6 ">
        {/* <Image width='11' src={logoPath} alt='cortxLogo' */}
        <Image src={'cortx_image.png'} alt="cortxLogo" className="sm:absolute w-40 sm:w-fit" />
      </div>
      <AnimatePresence>
        <motion.div
          initial={false}
          animate={ipfsStore.selectedIdx.length != 0 ? 'visible' : 'hidden'}
          exit={{ opacity: 0 }}
          transition={{ ease: "easeInOut", duration: .5 }}
          variants={{
            visible: { opacity: 1, y: 0 },
            hidden: { opacity: 0, y: 500 },
          }}
        >
          <div
            className="w-4/5 sm:w-fit flex flex-col bg-gradient-to-b from-slate-900 border-t border-b border-lime-400 text-snow px-6 py-3 mb-6 sm:mx-auto"
            role="alert"
          >
            <p className="font-semibold">Available Buckets</p>
            <p className="text-sm ">Select a bucket to upload your IPFS files.</p>
          </div>
          <div className="flex sm:flex-col overflow-x-scroll z-50 scrollbar-hide">
            {store.buckets?.length > 0 ? (
              store.buckets.map((bucket, i) => {
                return <BucketCard bucket={bucket} idx={1} key={uuid()} />
              })
            ) : isLoading ? (
              <BezierSpinner />
            ) : (
              <div
                className="mx-auto" //  TODO:animate bounce'
              >
                No Buckets ☠️
              </div>
            )}
          </div>
        </motion.div>
      </AnimatePresence>
    </div>
  )
}
