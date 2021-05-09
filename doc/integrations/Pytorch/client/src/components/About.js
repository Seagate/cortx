import React, { useEffect, useState } from 'react'
import ReactMarkdown from 'react-markdown';
import {toast} from 'react-toastify';
import './home.css'
import about from './about.md'
import axios from 'axios';
import ReactEmbedGist from 'react-embed-gist';
import { Link } from 'react-router-dom';


export default function About() {

    const [markdown,setMarkdown]=useState('## abhay');
    const [loading,setLoading]=useState(false);

    

    // useEffect(()=>{
    //     axios.get(about).then((res)=>{
    //         console.log(res);
    //         setMarkdown(res.data)
    //         setLoading(false);
    //     }).catch(err=>{
    //         toast.error('Something went wrong.Please try again', {
    //             position: "top-right",
    //             autoClose: 5000,
    //             hideProgressBar: false,
    //             closeOnClick: true,
    //             pauseOnHover: true,
    //             draggable: true,
    //             progress: undefined,
    //           });
    //         setLoading(false);
    //     })
    // },[])

    
    return loading ? (
        <div class="d-flex justify-content-center" style={{marginTop:'5rem',}}>
      
    <div class="col-sm-6 text-center"><p>Loading ...</p>
      <div class="loader4"></div>
    
    </div>
    
  </div>
    ):(
        <div className="container" style={{marginTop:'5rem',marginBottom:'3rem'}}>
        <h3 style={{alignSelf:'center'}}>Euclid - a REST API for CORTX S3 with Pytorch integration</h3>
        <p>
        What is Euclid ?
        <br></br>
        <br></br>
        Euclid is a REST API for CORTX s3, which allows users to add,remove, download data from CORTX S3 with simple GET and POST requests.
        <br></br>
        As data is stored with great efficieny,large amount of data can be stored and retrieved easily.
        <br></br>
        <br></br>
        Users can use the website to add data or use scripts/code to add and retrieve data using our APIs.
        Using the APIs we show how the data can be pulled into any workspace and be used.
        <br></br>
        In this project we have shown a Pytorch integration with CORTX S3.
        Along with that Since Euclid is a REST API, can be used with any tool to get data,Leading room for a lot of integrations in the future.
        <br></br>
        <br></br>

        </p>
        <h6>Solution </h6>
        <ul>
            <li>Use Euclid API to add/delete/download data from CORTX S3.</li>
            <li>Use this API to get data into any workspace or project or tool.(Eg A python workspace, using pytorch library)</li>
        </ul>
        
        
        <h6>Inspiration</h6>
        <ul>
        <li> Using the power of CORTX S3 to provide a better way for storage of Objects.</li>
        <li>Easy to use platform for uploading,retrieving data with UI and also API endpoints</li>
        <li>Enabling Developers/Researchers and others to easiy store data and use them in their applications.</li>
        </ul>
        <img className="img-fluid" src="https://i.ibb.co/9cZhF3h/17.png"></img>
        <img className="img-fluid" src="https://i.ibb.co/JFd9Kn1/19.png"></img>
        

        
        
       
        
        <h6>Introduction</h6>
        <p>PyTorch is an open source machine learning library based on the Torch library, used for applications such as computer vision and natural language processing, primarily developed by Facebook's AI Research lab.</p>

        <a href="">Concept pitch and Integration walk through</a>
        <ReactEmbedGist gist="abhayrpatel10/9f1d24df9f0deeacead2d0ef6e78b847"/>

       
        
        
        
    
        <img className="img-fluid" src="https://i.ibb.co/0mcpvPv/18.png"></img>
        
        

        
        <script src="https://gist.github.com/abhayrpatel10/9f1d24df9f0deeacead2d0ef6e78b847.js"></script>
        <h6>What's next</h6>
        <p>Using the REST API create more integrations </p>

        <h6>Created By </h6>
        <a href="https://github.com/abhayrpatel10">Abhay R Patel</a>
        <br></br>
        <a href="https://github.com/rishavrajjain">Rishav Raj Jain</a>
        
        
         
    </div>
    )
}
