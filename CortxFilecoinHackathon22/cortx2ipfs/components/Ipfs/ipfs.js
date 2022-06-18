import { useState, useEffect } from 'react'
import { create } from 'ipfs-http-client'

export function IpfsComponent(props) {
    const [id, setId] = useState(null);
    const [ipfs, setIpfs] = useState(null);
    const [version, setVersion] = useState(null);
    const [isOnline, setIsOnline] = useState(false);
    const [imgUrl, setUrl] = useState(null);

    useEffect(() => {
        const init = async () => {
            if (ipfs) return

            const node = await create({
                url: 'http://localhost:5001/api/v0',
                // url: 'http://127.0.0.1:45005/api/v0',
            });
            const nodeId = await node.id();
            const nodeVersion = await node.version();
            const nodeIsOnline = node.isOnline();

            setIpfs(node);
            setId(nodeId.id);
            setVersion(nodeVersion.version);
            setIsOnline(nodeIsOnline);
        }

        init()
    }, [ipfs]);

    // On button click, upload the selected file to IPFS and return the access URL
    async function handleClick() {
        // console.log(event.target)
        const file = props.state
        console.log(file)
        const res = await ipfs.add(file)
        console.log(res)
        setUrl(() => { return "https://ipfs.io/ipfs/" + res.path })
        setNewFile(() => true)
    }

    // Checks if a new file has been selected. If yes the old link should dissapear
    const [newFile, setNewFile] = useState(true);
    useEffect(() => {
        setNewFile(() => false)
    }, [props.state])

    if (!ipfs) {
        return <h4 className='f5 ma0 pb2 aqua fw4 montserrat'>Connecting to IPFS...</h4>
    }

    console.log(id)
    return (
        <div>
            <p>IPFS here:</p>
            <div>
                {props.state && <button onClick={handleClick} >Add to IPFS</button>}
            </div>
            <div>
                {imgUrl && newFile && (<a href={imgUrl}>Deployed to: {imgUrl}</a>)}
            </div>
            <h4 data-test="id">ID: {id.string}</h4>
            <h4 data-test="version">Version: {version}</h4>
            <h4 data-test="status">Status: {isOnline ? 'Online' : 'Offline'}</h4>

        </div>
    )
}

// export default IpfsComponent
