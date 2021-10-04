# Cortx Laravel Rest API

This repo is for us to integrate Cortx storage and turn it into another format, which is rest api and to be easily accessable by other parties if needed. Installation steps are written below. We are turning the CR(U)D operation into rest api so that other internet users can interact with ease as rest api are very popular in many major applications uses rest api to communicate with each others.

Possible use cases are
- Serving as a public file system so users can access these files with rest api, they can just go to the URL and get the data if they needed without using third part clients or knowing the credentials
- Having access control for different users to perform CRUD on the file system
- Integrate with other applications that uses rest api

## Installation Steps

1. Clone this [repo](https://github.com/shusiner/cortx-laravel-rest-api-bridge) and install it, guide can be found [here](https://laravel.com/docs/8.x/installation). This repo is using sail so if you have docker installed, you can just use the command ./vendor/bin/sail up in the repo directory. This is also mentioned in the above guide.

2. Install cortx VM, guide can be found [here](https://github.com/Seagate/cortx/blob/main/doc/ova/1.0.4/CORTX_on_Open_Virtual_Appliance.rst).

3. Fill in the value in the .env file, samples are provided here with the cortx VM.

- AWS_ACCESS_KEY_ID=AKIAzqwdJbVIQMa6NuZ_FSk7Ww
- AWS_SECRET_ACCESS_KEY=AK/rK4IGpVDF6s4Vo4hekKi7HEvBFzqs8gaivv5c
- AWS_DEFAULT_REGION=us-east-1
- AWS_BUCKET=laravel
- AWS_ENDPOINT=http://AKIAzqwdJbVIQMa6NuZ_FSk7Ww@192.168.1.160
- AWS_USE_PATH_STYLE_ENDPOINT=false

4. After repo is running, you can interact with the rest api by using api.http and visual studio code REST Client (humao.rest-client) by finding it in the extension marketplace. Demo video can be found [here](https://youtu.be/V3LY4AXekMc).

## Structure of the repo

The main file in the repo is in the api.php folder which shows all the routes available for the CRD operation and the guide to interact with laravel file system in found in [here](https://laravel.com/docs/8.x/filesystem).