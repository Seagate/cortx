import React from 'react'
import { Button } from 'antd'
import {CloudDownloadOutlined} from '@ant-design/icons'
import { downloadNotebookFile } from '../util'
import { ipfsUrl } from '../util/stor'

function DownloadNotebook({cid, fileName}) {

    const downloadNotebook = () => {
        downloadNotebookFile(ipfsUrl(cid, fileName))
    }

  const url = ipfsUrl(cid)

  return (
    <div>
        <h2>Download Notebook</h2>
        <a href={url} target="_blank">{url}</a>
        <p>Target file: <b>{fileName}</b></p>

        <Button className='standard-btn' onClick={downloadNotebook} type="primary">Download Notebook&nbsp;<CloudDownloadOutlined /></Button>
    </div>
  )
}

export default DownloadNotebook
