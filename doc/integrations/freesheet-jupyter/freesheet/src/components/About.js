import React from "react";
import PropTypes from "prop-types";
import logo from "../assets/logo.png";
import { APP_DESC, APP_NAME } from "../util/constants";
import { useNavigate } from "react-router-dom";
import { Button } from "antd";

function About(props) {
  const navigate = useNavigate()
  return (
    <div className="container">
      <img src={logo} />
      <br />
      <br />
      <h1>About</h1>
      <div>
        <h3>
          {APP_NAME}: {APP_DESC}
        </h3>
        <br/>

        <b>{APP_NAME} supports three main actions:</b>
        <br/>
        <br/>
<p>
  <ol>
    <li>Upload - Upload a csv of data either directly to your cortx instance or IPFS.  </li>
    <li>Exchange - push data sets effortlessly from your Cortx or AWS-hosted node to IPFS.  </li>
    <li>Access - Onboard your data sets immediately into Jupyter notebooks.  </li>
  </ol>
</p>

<div>
    <Button type="primary" className="standard-btn" size="large" onClick={() => navigate('/upload')}>Start Uploading</Button>
</div>

     </div>
    </div>
  );
}

About.propTypes = {};

export default About;
