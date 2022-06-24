### Cortx IPFS Bridge

## Get Started 🚀

Spin up an IPFS deamon:
```bash
ipfs daemon
```

Make sure your CORTX cluster is running and available. In case of [cloudshare](https://use.cloudshare.com/Authenticated/Landing.aspx?s=1), log in to your account.



## Resources

[AWS for React](https://docs.aws.amazon.com/sdk-for-javascript/v3/developer-guide/getting-started-react-native.html)

## IPFS

To spin up an IPFS node on the browser, use the `create()` from ipfs-core.
To connect to a running node on localhost, use the ipfs-http-client library like [here](https://github.com/ipfs/js-ipfs/tree/master/packages/ipfs-http-client)

The browser extension can not be connected to as it runs in Braves native IPFS mode, which refuses API connections.
Switching this to `local` will make it connectable.

[IPFS hooks](https://github.com/ipfs-examples/js-ipfs-examples/blob/master/examples/browser-create-react-app/src/App.js)
