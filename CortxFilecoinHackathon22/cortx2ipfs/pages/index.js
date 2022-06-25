// import IpfsComponent from "../components/Ipfs/ipfs";

import HomeWrapper from "./layout";
import { useEffect, useState } from 'react'
import IpfsReactComponent from "/components/Ipfs/ipfsReact";
import { IpfsComponent } from "/components/Ipfs/ipfs"
import { IpfsBox } from "../components/Ipfs/IpfsBox";

export default function Home() {

  return (
    <HomeWrapper>
      <IpfsBox>
        {/* <IpfsReactComponent></IpfsReactComponent> */}
        <IpfsComponent></IpfsComponent>
      </IpfsBox>
    </HomeWrapper>
  )
}
