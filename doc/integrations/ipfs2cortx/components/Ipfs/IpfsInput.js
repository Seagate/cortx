import { useState, useEffect } from 'react'
import {
  Box,
  Button,
  Input,
  InputGroup,
  InputLeftElement,
  Table,
  Text,
  Image,
} from '@chakra-ui/react'
import { useSelector, useDispatch } from 'react-redux'
import useMyToast from '../../hooks/useMyToast'
import { reset, setCid } from '../../app/ipfsReduxSlice'
import isIpfs from 'is-ipfs'
import { DopeAlter } from '../Alert/dopeAlert'
import { motion, AnimatePresence } from 'framer-motion'



export default function IpfsInput() {
  const store = useSelector((state) => state.ipfsRedux)
  const toast = useMyToast()
  const dispatch = useDispatch()

  function handleInput(event) {
    event.preventDefault()
    const currentCid = event.target.value
    if (isIpfs.cid(currentCid)) {
      // const currentCid = 'QmQ2r6iMNpky5f1m4cnm3Yqw8VSvjuKpTcK1X7dBR1LkJF'
      dispatch(setCid({ cid: currentCid }))
    } else {
      dispatch(reset())
    }
  }

  return (
    <>
      <Box className="m-3 sticky top-28 z-40">
        <DopeAlter headText={'Paste your IPFS CID:'} color={'aqua'} show={!store.cid} />
        <div>
          <InputGroup
            h="7"
          >
            <InputLeftElement
              h="7"
            >
              <Image alt="ipfsSmallBox" src="/ipfs-logo.svg" h={41} />
            </InputLeftElement>
            <Input
              h="7"
              rounded="xl"
              // fontWeight="black"
              onChange={(e)=> handleInput(e)}
              placeholder={store.cid || '<myCID>'}
              size="xs"
              variant="outline"
            ></Input>
          </InputGroup>
        </div>
      </Box>
    </>
  )
}
