## What is Images api?
The idea is to create an api on top of cortx to handle images optimization, basically an alternative to platforms like cloudinary, and provide open source solution for the community to use.

[visit the project repo](https://github.com/Seagate/cortx-images)
[project presentation](https://docs.google.com/presentation/d/1TvyV9C1GEHOv6lP1WLr7zU5KPcN192bMQxnnyfA00Oc/edit?usp=sharing)

### What is CORTX?
CORTX is a distributed object storage system designed for great efficiency, massive capacity, and high HDD-utilization. CORTX is 100% Open Source

### How do CORTX and Images API work together?
Because CORTX is S3 compatible we can use the storage system and the Images API feature to serve images in different sizes.

### Configuring Images API to work with CORTX:

Step 0: set environment variable, you can set it on the os level or create an .env file and set the variable in it, the variable needed is: 

    aws_access_key_id,
    aws_secret_access_key,
    bucket_name,
    endpoint_url,
    api_key

Step 1: assuming you have python 3;installed, run:

```
pip3 install -r requirements.txt
```

Step 2: Run the API server;
```
python3 api.py
```

#### Set up on Docker

Ensure you have docker installed in your system.

Step 1: Build the image

```
docker build --tag=cortx-images .
```

Step 2: Run the app; the .env file contains the app environment variables stated in step 0

```
docker run  --env-file .env -it -p 5000:5000 cortx-images
```

[setup video](https://www.loom.com/share/b81b6c973bac4a50872571eb0cec3e8c)

#### API endpoints

`GET /images/{filename}` // return original image

`GET /images/120x120/{filename}` // return resized image, with=120, height=120

- resized images will be stored for future use.

#### Watch the demo
   
![demo](https://github.com/Seagate/cortx-images/blob/master/static/demo.gif)

Tested By:
- Feb 26, 2022: Harrison Seow (harrison.seow@seagate.com) using OVA release 1.0.3.
