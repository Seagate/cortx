**What is Images api?**
The idea is to create an api on top of cortx to handle images optimization, basically an alternative to platforms like cloudinary, and provide open source solution for the community to use.

[visit the project repo](https://github.com/Seagate/cortx-images)

**What is CORTX?**
CORTX is a distributed object storage system designed for great efficiency, massive capacity, and high HDD-utilization. CORTX is 100% Open Source

**How do CORTX and Images API work together?**
Because CORTX is S3 compatible we can use the storage system and the Images API feature to serve images in different sizes.

**Configuring Images API to work with CORTX:**

*Step 0: set environment variable, you can set it on the os level or create an .env file and set the variable in it, the variable needed is: aws_access_key_id, aws_secret_access_key, bucket_name, endpoint_url, api_key*

*Step 1: assuming you have python 3 installed, pip3 install -r requirements.txt*

*Step 1: python3 api.py*

*if you prefer to use docker, there is a docker file ready in the project repo*

**Usage**

GET /images/{filename} // return original image

GET /images/120x120/{filename} // return resized image, with=120, height=120

GET / // return html page with the demo

* resized images will be stored for future use.
## Watch the demo 
   
![demo](https://github.com/Seagate/cortx-images/blob/master/static/demo.gif)
