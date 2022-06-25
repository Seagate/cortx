// This component  spins up a IPFS client on the brower. This is slower than than connecting to an existing IPFS node.

import useIpfsFactory from '/hooks/useIpfsFactory.js'
import useIpfs from '/hooks/useIpfs.js'
import { useEffect, useState } from 'react'



export default function IpfsReactComponent() {
  const { ipfs, ipfsInitError } = useIpfsFactory({ commands: ['id'] })
  const id = useIpfs(ipfs, 'id')
  const [version, setVersion] = useState(null)

  useEffect(() => {
    if (!ipfs) return;

    const getVersion = async () => {
      const nodeId = await ipfs.version();
      setVersion(nodeId);
    }

    getVersion();
  }, [ipfs])

  console.log(id)
  return (
    <div>
      {ipfsInitError && (
        <div className='bg-red pa3 mw7 center mv3 white'>
          Error: {ipfsInitError.message || ipfsInitError}
        </div>
      )}
      {(id || version) &&
        <section className='bg-snow mw7 center mt5'>
          <h1 className='f3 fw4 ma0 pv3 aqua montserrat tc' data-test='title'>Connected to IPFS</h1>
          <div className='pa4'>
            {id && <IpfsId obj={id} keys={['agentVersion']} />}
            {version && <IpfsId obj={version} keys={['version']} />}
          </div>
        </section>
      }
    </div>
  )
}


const Title = ({ text }) => {
  return (
    <h2 className='f5 ma0 pb2 aqua fw4 montserrat'>{text}</h2>
  )
}

const IpfsId = ({ keys, obj }) => {
  if (!obj || !keys || keys.length === 0) return null
  return (
    <>
      {keys?.map((key) => (
        <div className='mb4' key={key}>
          <Title>{key}</Title>
          <div className='bg-white pa2 br2 truncate monospace' data-test={key}>{key === 'id' ? obj[key].string : obj[key]}</div>
        </div>
      ))}
    </>
  )
}
