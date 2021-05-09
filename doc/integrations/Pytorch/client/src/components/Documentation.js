import React from 'react';
import './home.css';


export default function Documentation() {
    return (
        <div className="container" style={{marginTop:'5rem'}}>
        

        
        <div id="accordion">
        <div class="card">
          <div class="card-header" id="headingOne">
            <h5 class="mb-0">
              <button class="btn btn-link" data-toggle="collapse" data-target="#collapseOne" aria-expanded="true" aria-controls="collapseOne">
              <div className="row">
              <h5 style={{color:'green'}}>GET</h5>
              <p style={{marginLeft:'1rem'}}>List all files in the CORTX S3 </p>
              </div>
              
              </button>
              
                
            </h5>
          </div>
      
          <div id="collapseOne" class="collapse show" aria-labelledby="headingOne" data-parent="#accordion">
            <div class="card-body">
              <h6>URL : https://cortx.herokuapp.com/getFiles</h6>
            </div>
          </div>
        </div>
        <div class="card">
          <div class="card-header" id="headingTwo">
            <h5 class="mb-0">
              <button class="btn btn-link collapsed" data-toggle="collapse" data-target="#collapseTwo" aria-expanded="false" aria-controls="collapseTwo">
              <div className="row">
              <h5 style={{color:'blue'}}>POST</h5>
              <p style={{marginLeft:'1rem'}}>Add/Save a File to the CORTX S3</p>
              </div>
              </button>
            </h5>
          </div>
          <div id="collapseTwo" class="collapse" aria-labelledby="headingTwo" data-parent="#accordion">
            <div class="card-body">
                <h6>URL : https://cortx.herokuapp.com/save</h6>
                <p>curl --location --request POST 'https://cortx.herokuapp.com/save' \
                --form 'file=@"/test.csv"'</p>
                
            </div>
          </div>
        </div>
        <div class="card">
          <div class="card-header" id="headingThree">
            <h5 class="mb-0">
              <button class="btn btn-link collapsed" data-toggle="collapse" data-target="#collapseThree" aria-expanded="false" aria-controls="collapseThree">
              <div className="row">
              <h5 style={{color:'blue'}}>POST</h5>
              <p style={{marginLeft:'1rem'}}>Download a file from CORTX S3</p>
              </div>
              </button>
            </h5>
          </div>
          <div id="collapseThree" class="collapse" aria-labelledby="headingThree" data-parent="#accordion">
            <div class="card-body">
                <h6>URL : https://cortx.herokuapp.com/download</h6>
              <p>curl --location --request POST 'https://cortx.herokuapp.com/download' \
              --header 'Content-Type: application/json' \
              --data-raw {'{'}
                  "key":"test.csv"
              {'}'}'</p>
              <p>Key is the filename to be downloaded </p>
            </div>
          </div>
          
        </div>
        <div class="card">
          <div class="card-header" id="headingFour">
            <h5 class="mb-0">
              <button class="btn btn-link collapsed" data-toggle="collapse" data-target="#collapseFour" aria-expanded="false" aria-controls="collapseFour">
              <div className="row">
              <h5 style={{color:'red'}}>DELETE</h5>
              <p style={{marginLeft:'1rem'}}>Delete a file from CORTX S3</p>
              </div>
              </button>
            </h5>
          </div>
          <div id="collapseFour" class="collapse" aria-labelledby="headingFour" data-parent="#accordion">
            <div class="card-body">
                <h6>URL : https://cortx.herokuapp.com/delete</h6>
              <p>curl --location --request  DELETE 'https://cortx.herokuapp.com/delete' \
              --header 'Content-Type: application/json' \
              --data-raw {'{'}
                  "key":"test.csv"
              {'}'}'</p>
              <p>Key is the filename to be deleted </p>
            </div>
          </div>
          
        </div>
      </div>

      <section id="features" class="features">
        <div className="row">
        <div className="col-xl-12 col-lg-12 col-md-12 col-sm-12">
            <div class="icon-box" data-aos="zoom-in" data-aos-delay="50" >
            <i class="fa fa-file-text-o" style={{color: '#00A300',}}></i>
            <h3><a target="_blank" href="https://documenter.getpostman.com/view/5235222/TzJydG5r">View Complete Detailed API docs</a></h3>
            </div>
            
            
            </div>
        </div>
        </section>
        </div>
    )
}
