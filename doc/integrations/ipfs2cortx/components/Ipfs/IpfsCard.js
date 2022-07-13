import {
  Box,
  Button,
  List,
  ListIcon,
  ListItem,
  useColorMode,
  useColorModeValue,
} from '@chakra-ui/react'
import { useGetCidQuery, useLazyGetCidQuery } from '../../app/bridgeApi'
import useMyToast from '../../hooks/useMyToast'
import { useSelector, useDispatch } from 'react-redux'
import { useEffect, useState } from 'react'
import { IoSettings } from 'react-icons/io5'
import { v4 as uuid } from 'uuid'
import { BezierSpinner } from '../Spinner/BezierSpinner'
import { selectFile, unselectFile } from '../../app/ipfsReduxSlice'
import prettyBytes from 'pretty-bytes'

export const IpfsCard = ({ ls, idx }) => {
  const store = useSelector((state) => state.ipfsRedux)
  const dispatch = useDispatch()
  const toast = useMyToast()
  const [trigger, result, lastPromiseInfo] = useLazyGetCidQuery()

  const attrs = ['name', 'size', 'type']
  const spacing = 1
  const bg = useColorModeValue('bg-snow-muted', 'ring-1 ring-slate-900 bg-aqua-muted ')

  function onCardClick() {
    if (store.selectedIdx.includes(idx)) {
      dispatch(unselectFile({ idx }))
    } else {
      // Trigger the download of the clicked file
      trigger({ cid: ls.cid }, true)
    }
  }

  useEffect(() => {
    if (result.isSuccess) {
      const name = ls['name'] || 'ukwn' + uuid().toString()
      dispatch(selectFile({ idx, file: result.data, name }))
    } else if (result.isError) {
      console.log('🚀 ~ file: IpfsCard.js ~ line 43 ~ useEffect ~ result.isError', result.isError)
      toast('error', 'Failed to download file 😥', 'IpfsDownError')
    }
  }, [result, dispatch, ls, toast, idx])

  const hoverStyle = store.selectedIdx.includes(idx)
    ? ' bg-opacity-20 scale-110 hover:bg-opacity-10'
    : ' bg-opacity-10 hover:bg-opacity-20 hover:scale-105'

  const hiddenStyle = result.isLoading ? ' opacity-20' : ''
  const fileSize = ls['size'] ? prettyBytes(ls['size']) : 'unknown'

  return (
    <>
      <Box
        className={`${bg} p-1 mb-3 mx-3 rounded-xl shadow-xl transform-gpu transition duration-300 ease-in-out hover:cursor-pointer ${hoverStyle}`}
        onClick={onCardClick}
      >
        {result.isLoading && (
          <div className="fixed z-40 justify-center ml-11">
            <BezierSpinner
            // text={"DOWNLOADING..."}
            />
          </div>
        )}
        <div
          className={'flex flex-row z-10 prose-sm overflow-y-scroll scrollbar-hide' + hiddenStyle}
        >
          <List className="min-w-fit pl-0">
            {attrs.map((attr, i) => {
              return (
                <ListItem key={uuid()}>
                  <ListIcon as={IoSettings} className="fill-aqua" />
                  {attr + ':'}
                </ListItem>
              )
            })}
          </List>
          <List className="">
            {attrs.map((attr, i) => {
              return (
                <ListItem key={uuid()} title={ls[attr]?.length > 11 ? ls[attr] : null}>
                  {(attr === 'size' ? fileSize : ls[attr]) || 'unknown'}
                </ListItem>
              )
            })}
          </List>
        </div>
      </Box>
    </>
  )
}
