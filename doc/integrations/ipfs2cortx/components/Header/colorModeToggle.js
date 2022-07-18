import { IconButton, useColorMode, DarkMode } from '@chakra-ui/react'
import { useEffect } from 'react'
import { IoSunnyOutline, IoMoonOutline } from 'react-icons/io5'
import { useSelector, useDispatch } from 'react-redux'
import { toggleTheme } from '../../app/themeSlice'

export function ColorModeToggle() {
  const { colorMode, toggleColorMode } = useColorMode()
  const store = useSelector((state) => state.theme)
  const dispatch = useDispatch()

  // Toggle for tailwind. Src https://tailwindcss.com/docs/dark-mode
  function toggleButtonClick() {
    toggleColorMode()
    dispatch(toggleTheme)
  }

  return (
    <DarkMode>
      <IconButton
        className="ml-1"
        // bg="blueviolet"
        onClick={toggleButtonClick}
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
