const {pool} = require('../config')
const Busboy=require('busboy')
const busboy = require('connect-busboy');
const busboyBodyParser = require('busboy-body-parser');

AWS.config.loadFromPath('../config.json')
AWS.config.update({region: 'us-east-1'});

const s3=new AWS.S3({apiVersion: '2006-03-01'})

module.exports={
    getRadiology:(req,res)=>{
        pool.query('SELECT * FROM patients', (error, results) => {
            if (error) {
              throw error
            }
            res.status(200).json(results.rows)
          })
    },
    createRadiology:async (req,res,next)=>{
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
    }
}