import json
from flask import Flask, request, jsonify, send_file
import boto3
from botocore.client import Config
from werkzeug.utils import secure_filename
import os
from flask_cors import CORS

app = Flask(__name__)
CORS(app)


s3_resource = boto3.resource(
        's3', 
        region_name = 'us-west-2', 
        aws_access_key_id = os.environ.get('ACCESS_ID'),
        aws_secret_access_key = os.environ.get('SECRET_KEY'),
        endpoint_url=os.environ.get('ENDPOINT'),
        config=Config(signature_version='s3v4')
    ) 






@app.route('/save', methods=['POST'])
def save():
    
    file = request.files['file']
    

    

    filename = secure_filename(file.filename)

    
    
    result=s3_resource.Bucket("testbucket").put_object(
        Key = filename, 
        Body = file
    )

    return jsonify({'message': 'File uploaded'}, 200)

@app.route('/getFiles',methods=['GET'])
def getFiles():
    result={}
    
    bucket = s3_resource.Bucket('testbucket')
    for obj in bucket.objects.all():
        result[obj.key]=obj.last_modified
        
    return jsonify(result,200)

@app.route('/download',methods=['POST'])
def download():

    key = json.loads(request.data)['key']
    output = key
    s3_resource.Bucket('testbucket').download_file(key,output)




    

    return send_file(output,as_attachment=True)


@app.route('/delete',methods=['DELETE'])
def delete():
    key = json.loads(request.data)['key']
    result = s3_resource.Object('testbucket', key).delete()
    return jsonify(result,200)

   



if __name__ == "__main__":
    app.run()