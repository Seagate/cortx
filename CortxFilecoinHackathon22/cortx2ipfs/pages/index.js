// import IpfsComponent from "../components/Ipfs/ipfs";

import HomeWrapper from "./layout";
import { useEffect, useState } from 'react'
import IpfsReactComponent from "/components/Ipfs/ipfsReact";
import { IpfsComponent } from "/components/Ipfs/ipfs"
import { IpfsBox } from "../components/Ipfs/IpfsBox";
import { S3Box } from "../components/Aws/S3box";
import { S3React } from "../components/Aws/S3React";

export default function Home() {

  return (
    <HomeWrapper>
      <div 
      className="mr-11 justify-items-center justify-self-auto max-w-screen-md"
      >

      <IpfsBox>
        {/* <IpfsReactComponent></IpfsReactComponent> */}
        <IpfsComponent></IpfsComponent>
      </IpfsBox>
      <S3Box>
        <S3React></S3React>
      </S3Box>
      </div>
    </HomeWrapper>
  )
}
