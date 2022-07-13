// custom hook for toast
import { useToast } from '@chakra-ui/react'

export default function useMyToast() {
  const toast = useToast()
  function call(status, description, toastId) {
    if (!toast.isActive(toastId)) {
      toast({
        id: toastId,
        duration: 3300,
        status: status,
        description: description,
        position: 'bottom',
        // isClosable: true,
      })
    }
  }

  return call
}
