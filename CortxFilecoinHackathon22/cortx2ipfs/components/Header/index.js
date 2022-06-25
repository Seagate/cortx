// import { Disclosure } from '@chakra-ui/react'
import { HiMenu, HiX } from 'react-icons/hi'
import ColorModeToggle from './colorModeToggle'
import { Box, Text, DarkMode, Button, VStack } from '@chakra-ui/react'
import Image from 'next/image'

export default function Header() {
    return (
        <>
            <Box as="nav" className="bg-neutral-900 shadow-2xl z-10 opacity-100 sticky top-0 ">
                <Box className=" mx-auto px-2 sm:px-6 lg:px-8">
                    <Box className="relative flex items-center justify-between h-16">
                        <Box className="absolute inset-y-0 left-0 flex items-center sm:hidden">
                        </Box>
                        <Box className="flex-1 flex items-center justify-center sm:items-stretch sm:justify-start">
                            <Box className="flex-shrink-0 flex items-center text-white">
                                {/* <img
                                    className="h-6 w-auto"
                                    src="https://supabase.com/images/logo-dark.png"
                                    alt="supabase"
                                /> */}
                                <Image
                                    width={291}
                                    height={70}
                                    src="/cortx_challenge.png"
                                />
                            </Box>
                            <Box className="hidden sm:block sm:ml-6">
                            </Box>
                        </Box>
                        <Box className="absolute inset-y-0 right-0 flex items-center pr-2 sm:static sm:inset-auto sm:ml-6 sm:pr-0">
                            {/** notifications */}
                            {/* {AuthUser() ? <MenuLogado user={user} /> : <MenuNotLogado />} */}
                            <ColorModeToggle />
                        </Box>
                    </Box>
                </Box>
            </Box>
        </>
    )
}
