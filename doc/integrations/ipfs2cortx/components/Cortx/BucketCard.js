import {
  Box,
  Button,
  Icon,
  List,
  ListIcon,
  ListItem,
  useColorMode,
  useColorModeValue,
} from '@chakra-ui/react'
import useMyToast from '../../hooks/useMyToast'
import { useSelector, useDispatch } from 'react-redux'
import { useEffect, useState } from 'react'
import prettyBytes from 'pretty-bytes'
import { v4 as uuid } from 'uuid'
import { selectBucket, useLazyDeploy } from '../../app/cortxSlice'
import { usePostFiles2BucketMutation, useGetBucketFilesQuery } from '../../app/bridgeApi'

export const BucketCard = ({ bucket, idx }) => {
  const store = useSelector((state) => state.cortx)
  const ipfsStore = useSelector((state) => state.ipfsRedux)
  const dispatch = useDispatch()
  const toast = useMyToast()
  const [postFiles, result] = usePostFiles2BucketMutation()

  // TODO: list files and size for each bucket
  const attrs = ['size', 'files']
  const spacing = 1
  const { data: bucketFiles, error, isLoading } = useGetBucketFilesQuery({ bucket })
  // const bg = useColorModeValue('bg-lime-300 font-bold', 'ring-1 ring-slate-900 bg-lime-700 ')

  async function onCardClick() {
    dispatch(selectBucket({ bucket }))
    toast('info', 'Uploading Files to ' + bucket, 'infoBucket')
    for (let i = 0; i < ipfsStore?.selectedFiles.length; i++) {
      // TODO: send in batches
      const objName = ipfsStore.selectedName[i]
      // TODO: make reducer with status toast
      postFiles({ objName, objData: ipfsStore.selectedFiles[i], bucketName: bucket })
    }
  }

  useEffect(() => {
    if (result.isSuccess) {
      toast('success', 'Uploaded File to ' + bucket, 'infoBucket 🎉')
    } else if (result.isError) {
      toast('error', 'File could not be uploaded to ' + bucket, ' ☠️')
    }
  }, [result, toast, bucket])

  const hoverStyle =
    store.selectedBucket === bucket ? ' sm:translate-x-3 ' : ' sm:hover:translate-x-3 '

  const fileSize = bucket.size ? prettyBytes(ls['size']) : '11.3 Mb'

  const [nFiles, setNFiles] = useState()
  const [bucketSize, setBucketSize] = useState()

  useEffect(() => {
    if (bucketFiles) {
      const nFiles = bucketFiles?.objects?.Contents?.length
      let totalSize = 0

      for (let i = 0; i < nFiles; i++) {
        totalSize += bucketFiles?.objects?.Contents[i].Size
      }
      setBucketSize(() => prettyBytes(totalSize))
      setNFiles(() => nFiles)
    }
  }, [bucketFiles])

  return (
    <>
      <Box
        className={`flex flex-row prose-sm mx-1 sm:mx-16 mb-6 rounded-lg bg-gradient-to-b from-lime-500 bg-opacity-50 shadow-2xl transform-gpu transition duration-300 ease-in-out hover:cursor-pointer ${hoverStyle}`}
        onClick={onCardClick}
      >
        <Box
        // className={` p-1${hoverStyle}`}
        >
          <h4 className="text-right uppercase font-mono ml-3 my-auto mr-auto shadow-2xl">
            {bucket}
          </h4>
        </Box>
        <div className={' text-right flex flex-row z-10 overflow-scroll scrollbar-hide mr-0 pr-3'}>
          <List className="w-11 pl-0 mr-0">
            {attrs.map((attr, i) => {
              return (
                <ListItem key={uuid()}>
                  {/* <ListIcon as={IoSettings} className="fill-aqua" /> */}
                  {attr + ':'}
                </ListItem>
              )
            })}
          </List>
          <div className="mr-0">
            <List className="">
              {attrs.map((attr, i) => {
                return (
                  <ListItem key={uuid()}>
                    {(attr === 'size' ? bucketSize : nFiles) || '❔'}
                  </ListItem>
                )
              })}
            </List>
          </div>
        </div>
      </Box>
    </>
  )
}
