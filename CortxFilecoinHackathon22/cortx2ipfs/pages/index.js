// import IpfsComponent from "../components/Ipfs/ipfs";

import HomeWrapper from "./layout";
import { useEffect, useState } from 'react'
import { IpfsBox } from "../components/Ipfs/IpfsBox";
import { S3Box } from "../components/Aws/S3box";
import { S3React } from "../components/Aws/S3React";
import IpfsInput from "../components/Ipfs/IpfsInput";

export default function Home() {

  return (
    <HomeWrapper>
      <div
        className="flex mr-11"
      >

        <IpfsBox>
          <IpfsInput></IpfsInput>
        </IpfsBox>
        <S3Box>
          <S3React></S3React>
        </S3Box>
      </div>
    </HomeWrapper>
  )
}
