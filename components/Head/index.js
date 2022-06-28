import { default as HeadContainer } from 'next/head'

const Head = () => {
  return (
    <div>
      <HeadContainer>
        <title>IPFS 2 CORTX - by 3llobo</title>
        <meta name="description" content="Bridge between IPFS and CORTX." />
        <meta name="viewport" content="initial-scale=1.0, width=device-width" />
      </HeadContainer>
    </div>
  )
}

export default Head
