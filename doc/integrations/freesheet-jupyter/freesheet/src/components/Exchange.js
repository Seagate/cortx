import { Button, Input, Spin, Table } from "antd";
import React, { useState, useEffect } from "react";
import CSVReader from "react-csv-reader";
import { useNavigate } from "react-router-dom";
import { getFileName } from "../util";

import { getObject, listObjects, uploadRows } from "../util/api";
import { APP_NAME, DEFAULT_BUCKET } from "../util/constants";
import { storeFiles } from "../util/stor";

function Exchange({bucket}) {
  const [loading, setLoading] = useState(false);
  const [onboardLoading, setOnboardLoading] = useState(false)
  // const [data, setData] = useState();
  const [error, setError] = useState()
  const [objects, setObjects] = useState()
  const [cid, setCid] = useState()
  const [fileName, setFileName] = useState()
  const navigate = useNavigate()

  const list = async () => {
    if (!bucket) {
      alert('Bucket name is required')
      return
    }
    setLoading(true)

    try {
      const res = await listObjects(bucket)
      const files = res.data.Contents
      console.log('files', files)
      setObjects(files)
    } catch (e) {
      console.error(e)
    } finally {
      setLoading(false)
    }
  }

  // Push the file from the AWS instance to IPFS.
  const pushFile = async (objectEntry) => {
    setOnboardLoading(true)
    let result
    const fName = getFileName(objectEntry.Key)
    console.log('pushFile', bucket, fName)

    try {
      // TODO: push file to IPFS and render onboard button + IPFS url.

      const obj = await getObject(bucket, fName)

      console.log('getObject', obj)
      const contents = obj.data.Body.data.map(x => String.fromCharCode(x));
      const newFile = new File(contents, fName)

      result = await storeFiles([newFile]);
      // Once file pushed to IFPS, set the active cid.
      setCid(result)
      setFileName(fName)
    } catch (e) {
      const msg = e.message || e.toString()
      console.error('Check your web3.storage key: ' + msg)
      alert("Error uploading file: " + msg)
    } finally {
      setOnboardLoading(false)
      return;
    }
  }

    const columns = [
      {
        title: 'Key',
        dataIndex: 'Key',
        key: 'Key',
      },
      {
        title: 'Last Modified',
        dataIndex: 'LastModified',
        key: 'LastModified',
      },
      {
        title: 'Size (bytes)',
        dataIndex: 'Size',
        key: 'Size',
      },
      {
        title: 'Push to IPFS',
        key: 'onboard',
        render: r => {
          return <Button loading={onboardLoading} disabled={onboardLoading} onClick={() => pushFile(r)}>Push</Button>
        }
      },
    ];

  return (
    <div className="container">
      {/* <Input prefix="Bucket:" value={bucket} onChange={e => setBucket(e.target.value)}/> */}
      <h1>Exchange data between CortX and IPFS</h1>
      {!bucket && <p className="error-text">No bucket selected</p>}
      {/* <p>Use this page to exchange files between Cortx and IPFS</p> */}
      <Button type="primary" className="standard-btn" disabled={loading || !bucket} onClick={list}>Load objects</Button>
      {bucket && <p><b>Active bucket: {bucket}</b></p>}
      <Table loading={loading} dataSource={objects || []} columns={columns} />

      {cid && <Button type="primary" className="standard-btn" onClick={() => navigate(`/onboard/${cid}/${fileName}`)}>View IPFS entry</Button>}
    </div>
  );
}

export default Exchange;
