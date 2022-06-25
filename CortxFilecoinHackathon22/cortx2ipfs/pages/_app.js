import '../styles/globals.css'
import LogRocket from 'logrocket';
import { NextUIProvider } from '@nextui-org/react';




function MyApp({ Component, pageProps }) {

  LogRocket.init('8gay9t/wolf');
  // This is an example script - don't forget to change it!
  LogRocket.identify('WOLF', {
    name: 'Wolf',
    email: 'wolf@example.com',

    // Add your own custom user variables here, ie:
    type: 'dev'
  });

  return (
    <NextUIProvider>
      <Component {...pageProps} />
    </NextUIProvider>
  );
}

export default MyApp
