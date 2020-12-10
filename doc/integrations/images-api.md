# Cortx images
## Intro

The idea is to create an alternative to platforms like cloudinary, and provide open source solution for the community to use.

### Usage

you can use the provided docker file to build and run the project.

GET /images/{filename} // return original image

GET /images/120x120/{filename} // return resized image, with=120, height=120

* resized images will be stored for future use.
* env variable you should set: aws_access_key_id, aws_secret_access_key, bucket_name, endpoint_url, api_key

![demo](https://github.com/niradler/cortx-images/blob/master/static/demo.gif)
