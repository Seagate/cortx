import {
  Box,
  Button,
  List,
  ListIcon,
  ListItem,
  useColorMode,
  useColorModeValue,
} from '@chakra-ui/react'
import { useLsCidQuery } from '../../app/bridgeApi'
import useMyToast from '../../hooks/useMyToast'
import { useSelector, useDispatch } from 'react-redux'
import { useEffect } from 'react'
import { IoSettings } from 'react-icons/io5'
import { v4 as uuid } from 'uuid'
import { IpfsCard } from './IpfsCard'
import { BezierSpinner } from '../Spinner/BezierSpinner'
import { resetFile } from '../../app/ipfsSlice'

export default function IpfsLs() {
  const store = useSelector((state) => state.ipfsRedux)
  const dispatch = useDispatch()
  const toast = useMyToast()
  const { data, error, isLoading, isError } = useLsCidQuery({ cid: store.cid })

  useEffect(() => {
    if (isError) {
      toast('error', 'No files found related to this CID 💔', 'ipfsCidError')
      console.log('🚀 ~ file: IpfsLs.js ~ line 11 ~ IpfsLs ~ error', error)
      dispatch(resetFile())
    }
  }, [isError, dispatch, toast, error])

  return (
    <Box className="">
      {isLoading ? (
        <BezierSpinner></BezierSpinner>
      ) : (
        !isError && (
          <Box className="relative flex flex-col">
            <div className={store.selectedIdx.length != 0 ? 'opacity-0' : ''}>
              <p className="min-w-fit mr-3 align-text-bottom my-1 font-semibold text-center">
                Select files to upload:
              </p>
            </div>
            <div className="flex sm:flex-col overflow-x-scroll scrollbar-hide z-20">
              {data.map((file, i) => {
                return <IpfsCard ls={file} idx={i} key={uuid()} />
              })}
            </div>
          </Box>
        )
      )}
    </Box>
  )
}
