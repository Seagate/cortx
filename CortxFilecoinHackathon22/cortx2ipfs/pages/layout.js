import Image from 'next/image'
import Header from '../components/Header'
import { IpfsComponent } from "/components/Ipfs/ipfsClient"



export default function HomeWrapper({ children }) {
  return (
    <div>
      <Header />
      <main>
        {children}
      </main>

      <footer>
        <IpfsComponent></IpfsComponent>

        <a
          target="_blank"
          rel="noopener noreferrer"
        >
          <span>
            <Image src="/vercel.svg" alt="Vercel Logo" width={72} height={16} />
          </span>
        </a>
      </footer>
    </div>
  )
}
