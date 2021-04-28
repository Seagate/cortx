# CORTX-DRP Integration

## Background

**What is DRP?**<br/>
DRP stands for Declarative Resource Protocol.  It is an open source service mesh project with data mesh extensions.  While originally developed for use with infrastructure sources, it was adapted for use with mock civic data sets using CORTX storage for this project.

## Demo Video
A quick video describing this project, what DRP is and how it leverages CORTX storage [Demo on YouTube](https://youtu.be/_fWgcD-Y-G4).

## Project Repository
The DRP project [GitHub Repo](https://github.com/adhdtech/DRP).

## Demo Steps

### Set up CORTX Storage
Prepare your environment and have the Endpoint, AccessIDKey and SecretAccessKey values ready.

### Run Docker Container
Run the adhdtech/drp-nodejs container with "node serverCortx.js".  Substitute the S3ENDPOINT, S3ACCESSKEYID and S3SECRETACCESSKEY values with your 

```
> docker run --name drpcortx -P 8080 -e REGISTRYURL=ws://localhost:8080 -e PORT=8080 -e S3ENDPOINT=https://192.168.5.148 -e S3ACCESSKEYID=AKIAT7y9JwKbREyMsCX08G3blg -e S3SECRETACCESSKEY=IHQRZ8U/06tboLFyLduRdLfWvkSt9gl1rHQUeATQ adhdtech/drp-nodejs node serverCortx.js
```

The service should now be listening on port 8080 for you to access with your browser.

### Open DRP Desktop
In a browser, go to https://localhost:8080.  Enter any username and password to get access.

### View Mesh Topology
Once the DRP Desktop is open, click Go -> DRP Topology to view the microservices on the mesh.

### Class Data
The sample microservices advertise class definitions to the mesh.  Class records are retrieved from CORTX then dynamically related using UML field stereotypes.  These records are visible in Go -> Hive Browser.  Type "Springfield" in the Search box and hit Enter.

### Streaming Data
Streaming data emitted from services can be watched in Go -> DRP Shell.
```
dsh> watch firedept
Subscribed to stream firedept

[firedept] {"TimeStamp":"20210428065806","Message":"a random fire event","Route":["adb571bf7456-1"]}
[firedept] {"TimeStamp":"20210428065811","Message":"a random fire event","Route":["adb571bf7456-1"]}
```

## Author
- **Pete Brown**
