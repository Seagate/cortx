const express = require('express')
const bodyParser = require('body-parser')
const cors = require('cors')
const {pool} = require('./config')
const chatbot=require('./chatbot/Chatbot')
const AWS=require('aws-sdk')
let config = require('./config.json')

let dataLocation="";
// const { response } = require('express')



const Busboy=require('busboy')
const busboy = require('connect-busboy');
const busboyBodyParser = require('busboy-body-parser');

const s3=new AWS.S3({
  accessKeyId:config.accessKeyId,
  secretAccessKey:config.secretAccessKey,
  region:config.region
})

const greenEndpoint = new AWS.Endpoint('http://uvo112mdjilnxce3gc0.vm.cld.sr');

const s3v1 = new AWS.S3({
    endpoint: greenEndpoint,
    accessKeyId: 'AKIAtEpiGWUcQIelPRlD1Pi6xQ',
    secretAccessKey:'YNV6xS8lXnCTGSy1x2vGkmGnmdJbZSapNXaSaRhK',
    s3ForcePathStyle: true,
  });



const app = express()

app.use(bodyParser.json())
app.use(bodyParser.urlencoded({extended: false}))

app.use(busboy());
app.use(busboyBodyParser());
app.use(cors())

const getDoctor = (request, response) => {
  pool.query('SELECT * FROM doctors', (error, results) => {
    if (error) {
      throw error
    }
    response.status(200).json(results.rows)
  })
}



app.get('/doctors',getDoctor)
  // GET endpoint
  app.post('/api/doctors',(req,res)=>{
    const {doctorID, firstname,lastname,speciality,sabbrev,gender,office,email,phone} = req.body

  const element1 = req.body.element1;
  var busboy = new Busboy({ headers: req.headers });
  // The file upload has completed
  busboy.on('finish', async function() {
   console.log('Upload finished');
   
   
   
   const file=req.files.filetoupload;
  //  console.log(innerfil)
   console.log(file);

   var uploadParams = {Bucket: "zee-flask-s3-test", Key: file.name, Body: file.data};


await s3.upload (uploadParams, function (err, data) {
if (err) {
  console.log("Error", err);
} if (data) {
  console.log("Upload Success", data.Location);
  let newlocation=data.Location
  

  // postgresql insert
  // const {firstname,lastname,email,phone}=req.body;
  pool.query(
    'INSERT INTO doctors (doctorID,firstname,lastname,speciality,sabbrev,gender,office,email,phone,photo) VALUES ($1, $2,$3,$4,$5,$6,$7,$8,$9,$10)',
    [doctorID,firstname,lastname,speciality,sabbrev,gender,office,email,phone,newlocation],
    (error) => {
      if (error) {
        throw error
      }
      res.status(200).json({status: 'success', message: 'New dialogue added',data:data})
    },
  )
  // pool.query(
  //   'INSERT INTO patients (firstname,lastname,email,phone,image) VALUES ($1,$2,$3,$4,$5)',[firstname,lastname,email,phone,newlocation],(err,data)=>{
  //     if(err){
  //       throw err;
  //     }

  //     res.status(200).json({status: 'success', message: 'New dialogue added',data:data})
      
  //   }
  // )
  
}
});
  });
  req.pipe(busboy);

  })
  // POST endpoint
  

app.post('/update',(req,res)=>{
  const {doctorid,firstname,lastname,speciality,sabbrev,gender,office,email,phone,photo}=req.body;
  console.log(req.body)
  pool.query(`UPDATE doctors SET doctorID=$1,firstname=$2,lastname=$3,speciality=$4,sabbrev=$5,gender=$6,office=$7,email=$8,phone=$9,photo=$10 where doctorID=$1`,[doctorid,firstname,lastname,speciality,sabbrev,gender,office,email,phone,photo],(error,results)=>{
    if(error){
        throw error;
    }
    res.status(200).json(results.rows)
})

})

  // Chatbot routes
/*   app.get('/',(req,res)=>{
    res.writeHead(200, {'Content-Type': 'text/html'});
    res.write('<form action="/patients" method="post" enctype="multipart/form-data">');
    res.write('<input type="file" name="filetoupload"><br>');
    res.write('<input type="submit">');
    res.write('</form>');
    return res.end();
}) */

app.get('/logs',async (req,res)=>{
  
  pool.query(
    'SELECT * FROM logs',(error, results) => {
      if (error) {
        throw error
      }
      res.status(200).json(results.rows)
    }
  )
})

app.get('/radiology',(req,res)=>{
  pool.query('SELECT * FROM patients', (error, results) => {
    if (error) {
      throw error
    }
    res.status(200).json(results.rows)
  })
})

// Patients
app.get('/patients',(req,res)=>{
  pool.query('SELECT * FROM patients', (error, results) => {
    if (error) {
      throw error
    }
    res.status(200).json(results.rows)
  })
})
app.get('/upload',(req,res)=>{
  res.writeHead(200, {'Content-Type': 'text/html'});
  res.write('<form action="/api/upload" method="post" enctype="multipart/form-data">');
  res.write('<input type="file" name="filetoupload"><br>');
  res.write('<input type="text" name="firstname" placeholder="firstname"><br>');
  res.write('<input type="text" name="lastname" placeholder="lastname"><br>');
  res.write('<input type="text" name="email" placeholder="email"><br>');
  res.write('<input type="text" name="phone" placeholder="phone"><br>');
  res.write('<input type="submit">');
  res.write('</form>');
  return res.end();
})
app.post('/api/upload',async(req, res, next)=> {
  var params = {
    Bucket: "covidxraybucket"
   };
  await s3v1.createBucket(params, function(err, data) {
    if (err) console.log("Bucket already exits",err); 
    else     console.log(data);           
    
  });
  // This grabs the additional parameters so in this case passing     
  // in "element1" with a value.
  const element1 = req.body.element1;
  var busboy = new Busboy({ headers: req.headers });
  // The file upload has completed
  busboy.on('finish', async function() {
   console.log('Upload finished');
   
   
   
   const file=req.files.filetoupload;
  //  console.log(innerfil)
   console.log(file);
   const secondsSinceEpoch = Math.round(Date.now() / 1000)

   var uploadParams = {Bucket: "covidxraybucket", Key: `covid-${secondsSinceEpoch}.png`, Body: file.data};


await s3v1.upload (uploadParams, function (err, data) {
if (err) {
  console.log("Error", err);
} if (data) {
  console.log("Upload Success", data.Location);
  console.log(data)
  let newlocation=data.Location
  

  // postgresql insert
  const {firstname,lastname,email,phone}=req.body;
  pool.query(
    'INSERT INTO patients (firstname,lastname,email,phone,image) VALUES ($1,$2,$3,$4,$5)',[firstname,lastname,email,phone,newlocation],(err,data)=>{
      if(err){
        throw err;
      }

      res.status(200).json({status: 'success', message: 'New dialogue added',data:data})
      
    }
  )
  
}
});
  });
  req.pipe(busboy);
  // res.send("Upload event ended")
 });

app.post('/api/df_text_query',async(req,res)=>{

    let responses =await chatbot.textQuery(req.body.text,req.body.parameters);
    let d_response=responses[0].queryResult.fulfillmentText;
    let d_request=responses[0].queryResult.queryText;
    let d_skill_name=responses[0].queryResult.intent.displayName;
    let d_score=responses[0].queryResult.intentDetectionConfidence.toString()
    const now = new Date().toUTCString();
    var params = {
      Bucket: "logsbucket"
     };
    await s3v1.createBucket(params, function(err, data) {
      if (err) console.log("Bucket already exits",err); 
      else     console.log(data);           
      
    });
    
const newd= Date.now().toString;

    var obj = {
      Response: d_response,
      Request: d_request,
      IntentName:d_skill_name,
      Time:now,
      Score:d_score
  };
  
  var buf = Buffer.from(JSON.stringify(obj));
  
  var data = {
      Bucket: 'logsbucket',
      Key: `${newd}.json`,
      Body: buf,
      ACL: 'public-read'
  };
  
  await s3v1.upload(data, function (err, data) {
      if (err) {
          console.log(err);
          console.log('Error uploading data: ', data);
      } else {
          console.log('succesfully uploaded!!!');
          console.log(data)
      }
  });
    await pool.query(
      'INSERT INTO logs (time,querytext,text,skills,score) VALUES ($1,$2,$3,$4,$5)',
      [now,d_request,d_response,d_skill_name,d_score],
      (error) => {
        if (error) {
          throw error
        }
        console.log({status: 'success', message: 'New dialogue added'})
      },
    )

    res.send(responses[0].queryResult);
    
    
    
})

app.post('/api/df_event_query',async(req,res)=>{
    let responses =await chatbot.eventQuery(req.body.event,req.body.parameters);
    res.send(responses[0].queryResult);
})

app.get('/api/get_client_token', async (req, res) => {
  let token = await chatbot.getToken();
  res.send({token});
})


// Start server
app.listen(process.env.PORT || 5000, () => {
  console.log(`Server listening`)
})