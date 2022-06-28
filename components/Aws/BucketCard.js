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
import { selectBucket } from '../../app/cortxSlice'

export const BucketCard = ({ bucket, idx }) => {
  const store = useSelector((state) => state.cortx)
  const dispatch = useDispatch()
  const toast = useMyToast()

  const attrs = ['size', 'files']
  const spacing = 1
  // const bg = useColorModeValue('bg-lime-300 font-bold', 'ring-1 ring-slate-900 bg-lime-700 ')

  function onCardClick() {
    dispatch(selectBucket({ bucket }))
    toast('info', 'Uploading Files to ' + bucket, 'infoBucket')
  }

  const hoverStyle =
    store.selectedBucket === bucket ? ' sm:translate-x-3 ' : ' sm:hover:translate-x-3 '

  const fileSize = bucket.size ? prettyBytes(ls['size']) : '11.3 Mb'

  return (
    <>
      <Box
        className={`flex flex-row prose-sm max-w-xs mx-1 sm:mx-16 mb-6 rounded-lg bg-gradient-to-b from-lime-500 bg-opacity-50 shadow-2xl transform-gpu transition duration-300 ease-in-out hover:cursor-pointer ${hoverStyle}`}
        onClick={onCardClick}
      >
        <Box
        // className={` p-1${hoverStyle}`}
        >
          <h4 className="text-right uppercase font-mono ml-3 my-auto mr-auto shadow-2xl">
            {bucket}
          </h4>
        </Box>
        <div
          className={' text-right flex flex-row z-10 overflow-scroll scrollbar-hide ml-auto pr-3'}
        >
          <List className="min-w-fit pl-0">
            {attrs.map((attr, i) => {
              return (
                <ListItem key={uuid()}>
                  {/* <ListIcon as={IoSettings} className="fill-aqua" /> */}
                  {attr + ':'}
                </ListItem>
              )
            })}
          </List>
          <List className="">
            {attrs.map((attr, i) => {
              return (
                <ListItem key={uuid()}>
                  {(attr === 'size' ? fileSize : bucket[attr]) || '❔'}
                </ListItem>
              )
            })}
          </List>
        </div>
      </Box>
    </>
  )
}
