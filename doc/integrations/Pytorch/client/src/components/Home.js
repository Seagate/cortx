import React, { useEffect, useState } from 'react'
import './home.css'
import axios from 'axios';
import {toast } from 'react-toastify';
var fileDownload = require('js-file-download');

const Home=()=> {

    const colors=['#ffbb2c','#5578ff','#e80368']
    const [files,setFiles]=useState([]);
    const [loading,setLoading]=useState(true);
    const [file,setFile]=useState();
    useEffect(()=>{
      const getData= async()=>{
        axios.get(`${process.env.REACT_APP_BASE_URL_API}/getFiles`).then((res)=>{
        var keys= Object.keys(res.data[0])
        var values = Object.values(res.data[0])
        
        var filesData=[]
        for(var i=0;i<keys.length;i++){
          var temp={
            fileName:keys[i],
            dateModified:values[i]
          }
          filesData.push(temp);
        }

        setFiles(filesData)
        setLoading(false);
      }).catch(err=>{
        console.log(err)
        
      })
      }

      getData();
      
    },[])

    const download = async(fileName)=>{
      axios.post(`${process.env.REACT_APP_BASE_URL_API}/download`,{
        key:fileName
      },{
        responseType:'blob'
      }).then((res)=>{
        fileDownload(res.data, fileName)
        console.log(res);
        toast.success('Download successfull', {
          position: "top-right",
          autoClose: 5000,
          hideProgressBar: false,
          closeOnClick: true,
          pauseOnHover: true,
          draggable: true,
          progress: undefined,
        });
      }).catch((err)=>{
        console.log(err);
        toast.error('Something went wrong.Please try again', {
          position: "top-right",
          autoClose: 5000,
          hideProgressBar: false,
          closeOnClick: true,
          pauseOnHover: true,
          draggable: true,
          progress: undefined,
        });
      })
    }

    const deleteFile = async(fileName)=>{
      setLoading(true);
      axios.delete(`${process.env.REACT_APP_BASE_URL_API}/delete`,{
        headers: {
          'Content-Type': 'application/json',
        },
        data:{
          key:fileName
        }
      }).then((res)=>{
        toast.success('Deleted Successfully.Please refresh the page', {
          position: "top-right",
          autoClose: 5000,
          hideProgressBar: false,
          closeOnClick: true,
          pauseOnHover: true,
          draggable: true,
          progress: undefined,
        });
        setLoading(false);
      }).catch((err)=>{
        console.log(err)

        toast.error('Something went wrong.Please try again', {
          position: "top-right",
          autoClose: 5000,
          hideProgressBar: false,
          closeOnClick: true,
          pauseOnHover: true,
          draggable: true,
          progress: undefined,
        });
        setLoading(false);
      })
    }

    const saveFile = ()=>{
      var data = new FormData();
      setLoading(true);
      
      data.append('file',file)

      axios.post(`${process.env.REACT_APP_BASE_URL_API}/save`,data).then((res)=>{
        toast.success('File uploaded Successfully.Please refresh the page', {
          position: "top-right",
          autoClose: 5000,
          hideProgressBar: false,
          closeOnClick: true,
          pauseOnHover: true,
          draggable: true,
          progress: undefined,
        });
        setLoading(false);
        
      }).catch(err=>{
        toast.error('Something went wrong.Please try again', {
          position: "top-right",
          autoClose: 5000,
          hideProgressBar: false,
          closeOnClick: true,
          pauseOnHover: true,
          draggable: true,
          progress: undefined,
        });
        setLoading(false);
      })
    }

    const onFileChange=(e)=>{
      var files=e.target.files;
      console.log(files);
      var filesArr = Array.prototype.slice.call(files);

      console.log(filesArr.length)
      setFile(filesArr[0]);

      

     

  }


    return loading ? (<div class="d-flex justify-content-center" style={{marginTop:'5rem'}}>
      
    <div class="col-sm-6 text-center"><p>Loading ...</p>
      <div class="loader4"></div>
    
</div>
    
  </div>):(
        <div style={{marginTop:'3rem'}}>
        <section id="features" class="features">
        <div class="container">
  
          <div class="section-title" data-aos="fade-up">
            <p>Euclid</p>
            <h6>Add and Remove Immense amount of Data to cloud, powered by CORTX Storage Technology.Use this data in you application by just a simple POST request.</h6>
            
          </div>
  
          

          <div className="row" style={{marginTop:'2rem'}}>
            <div className="col-xl-3 col-lg-3 col-md-12 col-sm-12">
            <div class="icon-box" data-aos="zoom-in" data-aos-delay="50" >
            <i class="fa fa-plus" style={{color: '#00A300',}}></i>
            <h3><a data-toggle="modal" data-target="#exampleModal">Add File</a></h3>
            </div>
            
            
            </div>

            <div className="col-xl-9 col-lg-9 col-md-12 col-sm-12">
                {
                  files.map((file)=>{
                    return(
                      <div class="icon-box" data-aos="zoom-in" data-aos-delay="50" >
                      <i class="fa fa-file-text-o" style={{color: '#5578ff',}}></i>
                      <h3><a >{file.fileName}{'  '}</a></h3>
                      <div style={{marginLeft:'20px'}}>
                        <i class="fa fa-trash-o" style={{color: 'red',}} onClick={()=>deleteFile(file.fileName)}></i>
                        <i class="fa fa-download" onClick={()=>download(file.fileName)} style={{color: 'black',}}></i>
                      </div>
                      </div>
                    )
                  })
                }
            </div>


          </div>
  
        </div>
      </section>

      <div class="modal fade" id="exampleModal" tabindex="-1" role="dialog" aria-labelledby="exampleModalLabel" aria-hidden="true">
  <div class="modal-dialog" role="document">
    <div class="modal-content">
      <div class="modal-header">
        <h5 class="modal-title" id="exampleModalLabel">Upload File</h5>
        <button type="button" class="close" data-dismiss="modal" aria-label="Close">
          <span aria-hidden="true">&times;</span>
        </button>
      </div>
      <div class="modal-body">
      <div style={{alignItems:'center',justifyContent:'center',textAlign:'center',border:'3px dotted #8fd9ea',height:'200px',backgroundColor:'#d3e7ff'}}>
      <i class="fa fa-cloud-upload fa-4x" aria-hidden="true" style={{marginTop:'60px'}}></i>
      <br></br>
      <input style={{marginTop:'30px'}} type="file"  onChange={onFileChange} ></input>
  </div>
      </div>
      <div class="modal-footer">
        <button type="button" class="btn btn-secondary" data-dismiss="modal">Close</button>
        <button type="button" class="btn btn-primary" onClick={saveFile} data-dismiss="modal">Submit</button>
      </div>
    </div>
  </div>
</div>
        </div>
    )
}


export default Home;