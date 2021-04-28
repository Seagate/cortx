# CORTX-DRP Integration

## Background

**What is DRP?**<br/>
DRP stands for Declarative Resource Protocol.  It is an open source service mesh project geared toward infrastructure sources with data mesh extensions.

## Demo Video
A quick video describing what DRP is and how it leverages CORTX storage [Demo on YouTube](https://youtu.be/_fWgcD-Y-G4).

## Project Repository
The configuration file used in this integration is located in [GitHub Repo](https://github.com/adhdtech/DRP) repository.

## Demo Steps

### Set up CORTX Storage
Prepare your environment and have the Endpoint and credentials ready

### Run Docker Container
Run the DRP container with serverCortx.js using this format.  Substitute the S3ENDPOINT, S3ACCESSKEYID and S3SECRETACCESSKEY values with your 

```
> docker run --name drpcortx -P 8080 -e REGISTRYURL=ws://localhost:8080 -e PORT=8080 -e S3ENDPOINT=https://192.168.5.148 -e S3ACCESSKEYID=AKIAT7y9JwKbREyMsCX08G3blg -e S3SECRETACCESSKEY=IHQRZ8U/06tboLFyLduRdLfWvkSt9gl1rHQUeATQ adhdtech/drp-nodejs node serverCortx.js
```

The service should now be listning on port 8080 for you to access with your browser.

## Author
- **Pete Brown**
