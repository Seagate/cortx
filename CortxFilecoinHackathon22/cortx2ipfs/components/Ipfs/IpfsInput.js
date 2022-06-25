import { useState, useEffect } from 'react'
import { Box, Button, Input, InputGroup, InputLeftElement, Table, Text } from "@chakra-ui/react";
import { useSelector, useDispatch } from 'react-redux';
import { increment } from '../../app/counterSlice';
import Image from 'next/image';
import { getFile } from '../../app/ipfsSlice';

export default function IpfsInput() {

    const stateCid = useSelector((state => state.ipfsRedux.CID))
    const dispatch = useDispatch()

    function handleInput(event) {
        event.preventDefault();
        const currentCid = event.target.value
        console.log("🚀 ~ file: cidInput.js ~ line 14 ~ handleInput ~ currentCid", currentCid)
        // TODO: check for valid CID
        if (currentCid.length > 11) {
            dispatch(getFile(currentCid))
        }
    }

    return (
        <>
            <Box
                className='flex m-3'
            >

                <div
                    className='min-w-fit mr-3 align-text-bottom'
                >
                    IPFS CID here:
                </div>
                <div>
                    <InputGroup
                        h="7"
                    >
                        <InputLeftElement
                            h="7"
                            children={(
                                <Image
                                    layout='fill'
                                    // width={111}
                                    // height={111}
                                    src='/ipfs-logo.svg'
                                />
                            )}
                        />
                        <Input
                            h='7'
                            onChange={handleInput}
                            placeholder='<myCID>'
                        >
                        </Input>
                    </InputGroup>
                </div>
            </Box>
            <Box>
                {stateCid || ""}
            </Box>

        </>
    )
}