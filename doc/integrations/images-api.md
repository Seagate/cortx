**What is Images api?**
The idea is to create an api on top of cortx to handle images optimization, basically an alternative to platforms like cloudinary, and provide open source solution for the community to use.

**What is CORTX?**
CORTX is a distributed object storage system designed for great efficiency, massive capacity, and high HDD-utilization. CORTX is 100% Open Source

**How do CORTX and Images API work together?**
Because CORTX is S3 compatible we can use the storage system and the Images API feature to serve images in different sizes.

**Configuring Images API to work with CORTX:**

*set the following env variable: aws_access_key_id, aws_secret_access_key, bucket_name, endpoint_url, api_key*

Then use the attached docker file to run the project with zero configuration.

GET /images/{filename} // return original image

GET /images/120x120/{filename} // return resized image, with=120, height=120

* resized images will be stored for future use.
## Watch the demo 
   
![demo](https://github.com/niradler/cortx-images/blob/master/static/demo.gif)
