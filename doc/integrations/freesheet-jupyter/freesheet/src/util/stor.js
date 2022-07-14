// import { Web3Storage } from "web3.storage";
import { Web3Storage } from "web3.storage/dist/bundle.esm.min.js"; // webpack 4

const API_KEY = process.env.REACT_APP_STORAGE_KEY;
if (!API_KEY) {
  alert('REACT_APP_STORAGE_KEY environment variable is required')
}

function getAccessToken() {
  return API_KEY;
}

function makeStorageClient() {
  return new Web3Storage({ token: getAccessToken() });
}

export const ipfsUrl = (cid, fileName) => {
    let url = `https://ipfs.io/ipfs/${cid}`;
    if (fileName) {
      return `${url}/${fileName}`;
    }
    return url;
  };
  

export async function storeFiles(files) {
  const client = makeStorageClient();
  const cid = await client.put(files);
  console.log("stored files with cid:", cid);
  return cid;
}

export async function retrieveFiles(cid) {
  const client = makeStorageClient();
  const res = await client.get(cid);
  console.log(`Got a response! [${res.status}] ${res.statusText}`);
  if (!res.ok) {
    throw new Error(`failed to get ${cid}`);
  }

  // request succeeded! do something with the response object here...

  return res;
}