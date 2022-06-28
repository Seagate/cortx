export async function ipfsGetFile(node, Cid) {
  // Get a file from IPFS through its CID
  const stream = node.cat(Cid)
  const decoder = new TextDecoder()
  let data = ''

  for await (const chunk of stream) {
    // chunks of data are returned as a Uint8Array, convert it back to a string
    data += decoder.decode(chunk, { stream: true })
  }

  console.log(data)
  return data
}
