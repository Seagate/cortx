## Cortx-MyDrive is a free Open Source Cloud Server (Similar To Google Drive) integrated with CORTX engine
# ![Logo](logo.png)

notice ❤: this integration is heavily based on [cortx-js-sdk](https://github.com/kaminskypavel/cortx/blob/main/doc/integrations/cortx-js-sdk/README.md
)

in this repo we will demonstrate how it can be used in a big scaled projects. for more info click on the image below
![cortx-js-demo](https://github.com/kaminskypavel/cortx-js-sdk/raw/master/assets/logo.png)



## Youtube Demo
[![Demo](./youtube-play.png)](https://youtu.be/iegzyHMiqfw)

##### Host myDrive on your own server or trusted platform, and then access myDrive through your web browser.
----
## Screenshots

- #### upload documents
![upload](https://github.com/kaminskypavel/cortx-js-myDrive/blob/master/github_images/upload.png)

- #### download documents
![download](https://github.com/kaminskypavel/cortx-js-myDrive/blob/master/github_images/download.png)

- #### built in player
![player](https://github.com/kaminskypavel/cortx-js-myDrive/raw/master/github_images/video-viewer.png)

- #### search for documents
![search](https://github.com/kaminskypavel/cortx-js-myDrive/blob/master/github_images/move.png)


## Problem

In this day and age, privacy and "owning" your own information - matters!

Enterprises may choose a self hosted solutions for a well known software solutions, and some organizations might go as far as hosting their own servers and infrastructure.

## Solution 

We provide a self hosted solution for a remote data drive similar to "Google Drive" and "Dropbox".

## Technology 

- The project is base on the popular opensource project [myDrive](https://mydrive-storage.com/) who recieved over 2000+ stars.

[![GitHub stars](https://img.shields.io/github/stars/subnub/myDrive.svg?style=social&label=Star&maxAge=2592000)](https://GitHub.com/subnub/myDrive/stargazers/) 

- myDrive's code was integrated with `cortx-js-sdk` so all file uploads are stored on **CORTX object store**

## Instruction

1. checkout the code from the repository `$ git clone https://github.com/kaminskypavel/cortx-js-myDrive`

2. install dependencies `$ yarn`
   
3. make sure you're running a mongodb instance. you can run a local copy or use `docker run -it -v mongodata:/data/db -p 27017:27017 --name mongodb -d mongo`

4. create db indexes `yarn run create-indexes-database`

5. copy `config/dev.env.sample` to `config/dev.env` and fill in the S3 relevant ID , KEY , ENDPOINT etc..

5. run it! `yarn start:dev`
   
6. point your browser to `http://localhost:3000`

