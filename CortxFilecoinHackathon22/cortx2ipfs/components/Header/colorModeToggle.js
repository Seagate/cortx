import { IconButton, useColorMode, DarkMode } from '@chakra-ui/react'
import { IoSunnyOutline, IoMoonOutline } from 'react-icons/io5'

export function ColorModeToggle() {
    const { colorMode, toggleColorMode } = useColorMode()

    return (
        <DarkMode>
            <IconButton
                className="ml-1"
                // bg="blueviolet"
                onClick={toggleColorMode}
                aria-label="Toggle"
                _hover={{ bg: 'none' }}
                _active={{ bg: 'none' }}
                rounded="full"
                variant={'outline'}
            >
                {colorMode === 'light' ? <IoSunnyOutline /> : <IoMoonOutline />}
            </IconButton>
        </DarkMode>
    )
}

export default ColorModeToggle
