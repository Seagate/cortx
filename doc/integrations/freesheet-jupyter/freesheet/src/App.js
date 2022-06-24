import React, {useState, useEffect} from 'react'
import { Layout, Menu, Select } from "antd";
import { APP_NAME, DEFAULT_BUCKET } from "./util/constants";
import { Routes, Route, Link, Router } from "react-router-dom";
import UploadPage from "./components/UploadPage";
import About from "./components/About";
import Exchange from "./components/Exchange";
import Onboard from "./components/Onboard";
import logo from "./assets/logo.png";
import { listBuckets } from './util/api';

import "antd/dist/antd.min.css";
import "./App.css";
import { CheckCircleTwoTone, CloseCircleTwoTone } from '@ant-design/icons';
const { Option } = Select;


const { Header, Footer, Sider, Content } = Layout;

function App() {
  const [loading, setLoading] =useState(false)
  const [bucket, setBucket] = useState()
  const [buckets, setBuckets] = useState()

  const getBuckets = async () => {
    setLoading(true)
    try {
      const res = await listBuckets()
      setBuckets(res.data.Buckets.map(b => b.Name))
    } catch (e) {
      console.error('e', e)
    } finally {
      setLoading(false)

    }

  }


  useEffect(() => {
    getBuckets()
  }, [])


  return (
    <div className="App">
      <Layout>
        <Header className="header">
          <Menu theme="light" mode="horizontal" defaultSelectedKeys={["2"]}>
            <Link to="/">
              <Menu.Item key="0">
                <img src={logo} className="header-image" />
              </Menu.Item>
            </Link>
            <Link to="/upload">
              <Menu.Item key="1">Upload</Menu.Item>
            </Link>
            <Link to="/exchange">
              <Menu.Item key="2">Exchange</Menu.Item>
            </Link>
            <Link to="/onboard">
              <Menu.Item key="3">Onboard</Menu.Item>
            </Link>
            {/* <Link to="/about">
              <Menu.Item key="4">About</Menu.Item>
            </Link> */}
            <span>
            <Select placeholder="Select bucket" style={{ width: 120 }} value={bucket} onChange={(v) => setBucket(v)} loading={loading}>
              {(buckets || []).map((b, i) => {
                return <Option key={i} value={b}>{b}</Option>
              })}
            </Select>&nbsp;
            {!loading &&       <span>
              {buckets ? <span className='green'>
                Connected <CheckCircleTwoTone twoToneColor="#7CFC00" />
              </span> : 
              <span className='red'>
                Not Connected <CloseCircleTwoTone twoToneColor="#ff0000" />
              </span>}
            </span>}
            </span>
          </Menu>
        </Header>
        <Content>
          <Routes>
            <Route path="/" element={<About />} />
            <Route path="/upload" element={<UploadPage bucket={bucket} />} />
            <Route path="/exchange" element={<Exchange bucket={bucket || DEFAULT_BUCKET} />} />
            <Route path="/onboard/:cid/:fName" element={<Onboard bucket={bucket} />} />
            <Route path="/onboard" element={<Onboard bucket={bucket} />} />
            <Route path="/about" element={<About />} />
          </Routes>
        </Content>
        <Footer>
          {APP_NAME} &copy;2022 - Built for the Cortx 2022 Hackathon
        </Footer>
      </Layout>
    </div>
  );
}

export default App;
