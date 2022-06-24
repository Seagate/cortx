import '../styles/globals.css'
// import LogRocket from 'logrocket';
import { NextUIProvider } from '@nextui-org/react';
import { createTheme } from "@nextui-org/react"
import store from '../app/store'
import { Provider } from 'react-redux'




function MyApp({ Component, pageProps }) {

  const darkTheme = createTheme({
    type: 'dark',
    // theme: {
    //   colors: { ...}, // override dark theme colors
    // }
  });


  // LogRocket.init('8gay9t/wolf');
  // // This is an example script - don't forget to change it!
  // LogRocket.identify('WOLF', {
  //   name: 'Wolf',
  //   email: 'wolf@example.com',

  //   // Add your own custom user variables here, ie:
  //   type: 'dev'
  // });

  return (
    
    <Provider store={store}>
      <NextUIProvider >
        <Component {...pageProps} />
      </NextUIProvider>
    </Provider>
  );
}

export default MyApp
