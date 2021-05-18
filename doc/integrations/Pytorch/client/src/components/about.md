## Euclid - a REST API for CORTX S3 with Pytorch integration

### What is Euclid ?
Euclid is a REST API for CORTX s3, which allows users to add,remove, download data from CORTX S3 with simple GET and POST requests.
As data is stored with great efficieny,large amount of data can be stored and retrieved easily.

Users can use the website to add data or use scripts/code to add and retrieve data using our APIs.
Using the APIs we show how the data can be pulled into any workspace and be used.

In this project we have shown a Pytorch integration with CORTX S3.
Along with that Since Euclid is a REST API, can be used with any tool to get data,Leading room for a lot of integrations in the future.


### Solution
- Use Euclid API to add/delete/download data from CORTX S3.
- Use this API to get data into any workspace or project or tool.(Eg A python workspace, using pytorch library)

### Inspiration
- Using the power of CORTX S3 to provide a better way for storage of Objects.
- Easy to use platform for uploading,retrieving data with UI and also API endpoints
- Enabling Developers/Researchers and others to easiy store data and use them in their applications.

### Workflow
![Workflow](https://i.ibb.co/9cZhF3h/17.png)

### Integration
![Integration](https://i.ibb.co/JFd9Kn1/19.png)

### Introduction
PyTorch is an open source machine learning library based on the Torch library, used for applications such as computer vision and natural language processing, primarily developed by Facebook's AI Research lab.

### Concept pitch and Integration walk through
can be found [here]().

## Steps to run the app

```
1 Download all the projects files
2 Go to the Flask Server folder.Run pip install -r requirements.txt  
3 Start the server by running python app.py
```


## Flask Server Walk through
In a nutshell, The flask server connects to your CORTX S3 server and provides easy to use API.

```
s3_resource = boto3.resource(
        's3', 
        region_name = 'us-west-2', 
        aws_access_key_id = os.environ.get('ACCESS_ID'),
        aws_secret_access_key = os.environ.get('SECRET_KEY'),
        endpoint_url=os.environ.get('ENDPOINT'),
        config=Config(signature_version='s3v4')
    ) 
```
This connects to the CORTX S3 server.Set access id, secret key and endpoint as environment varibales or directly paste them in the code.View this link to set env variables [Link](https://stackoverflow.com/questions/5971312/how-to-set-environment-variables-in-python)

There are four Routes 
- /getFiles - GET REQUEST - lists all the files in the server.
- /save - POST REQUEST - send the file in formData with key as file.
- /download -POST REQUEST - send the filename as key - JSON-body
- /delete DELETE REQUEST - send the filename as key - JSON-body

[Complete Documentation with examples](https://cortx.netlify.app/docs)

use these APIs to store,download and manage data.
We also have  a UI to store,download and manage data. To use that go into the client folder 
```
1 run npm install
2 create a .env file in the client folder and paste REACT_APP_BASE_URL_API=http://127.0.0.1:5000 or the corresponding base URL if you have hosted the app.
3 run npm start
```
Use this UI to manage your data and also read the docs and much more.

## Pytorch Integration
Now using the data stored with pytorch

- create a Google Colab notebook and run this code
```
from google.colab import drive
drive.mount('/content/gdrive')
```
This mounts your Google drive.

```
import requests
list_of_files=requests.get(BASE_URL+'/getFiles')
print(list_of_files.content)
```
This gets the list of file in the server.

```
import requests
response = requests.post(BASE_URL+'/download',json={"key":"test.csv"})
open('test.csv', 'wb').write(response.content)
```
This downloads the data necessary file to create run and use models

Now we can use pytorch to create,train Neural Networks Easily.

[Check out the complete example here.](https://colab.research.google.com/drive/1ukYKEEyLMIRt5K4ci4-S5PULdc4vMk8o?usp=sharing)









## Tech Stack
![Tech STack](https://i.ibb.co/0mcpvPv/18.png)


## Demo and API links
[Demo Link - Click here](https://cortx.netlify.app/about)
[API docs](https://cortx.netlify.app/docs)

## What's next
- Using the REST API create more integrations 

## Created by 
[Abhay R Patel](https://github.com/abhayrpatel10)
[Rishav Raj Jain](https://github.com/rishavrajjain)


