import '../styles/globals.css'
import { store } from '../app/store'
import { Provider } from 'react-redux'
import { ChakraProvider } from '@chakra-ui/react'
import { myTheme } from '../styles/theme'

function MyApp({ Component, pageProps }) {
  return (
    <Provider store={store}>
      <ChakraProvider theme={myTheme}>
        <Component {...pageProps} />
      </ChakraProvider>
    </Provider>
  )
}

export default MyApp
