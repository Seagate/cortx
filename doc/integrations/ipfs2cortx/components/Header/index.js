// import { Disclosure } from '@chakra-ui/react'
import { HiMenu, HiX } from 'react-icons/hi'
import ColorModeToggle from './colorModeToggle'
import { Box, Text, DarkMode, Button, VStack, Image, useColorModeValue } from '@chakra-ui/react'
import { motion } from "framer-motion"


export default function Header() {
  const shadow = useColorModeValue('shadow-2xl', 'shadow-2xl')

  return (
    <>
      <Box
        as="nav"
        className={`bg-gradient-to-b from-slate-800 via-slate-900 to-neutral-900 ${shadow} z-30 opacity-100 sticky top-0`}
      >
        <Box className=" mx-auto px-2 sm:px-6 lg:px-8">
          <Box className="relative flex items-center justify-between h-16">
            {/* <Box className="absolute inset-y-0 left-0 flex items-center sm:hidden"></Box> */}
            <Box className="flex-1 flex items-center justify-center sm:items-stretch sm:justify-start">
              <Box className="flex-shrink-0 flex items-center text-white mr-6">
                <motion.div
                  animate={{
                    scale: [1, 2, 2, 1, 1],
                    rotate: [0, 0, 360, 360, 360],
                    borderRadius: ["20%", "20%", "50%", "50%", "20%"],
                  }}
                  transition={{ duration: 3 }}
                >
                  <Image height={66} src="/filecoin_cool.png" alt="FLsvg" />
                </motion.div>
              </Box>
              <Box className="flex-shrink-0 items-center text-white hidden sm:flex ">
                <Image height={35} src="/seagate-logo.svg" alt="CORTXsvg" />
              </Box>
              <Box className="hidden sm:block sm:ml-6"></Box>
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
