# cortx-here-traffic-alerts

Link to my video demo: https://www.youtube.com/watch?v=Ah8SAv5LvfA&ab_channel=DylanKearns

## Prerequisites
1. Create CORTX instance
 - Setting up the CORTX instance is pretty simple, just follow this [guide](https://github.com/Seagate/cortx/blob/main/doc/ova/2.0.0/PI-6/CORTX_on_Open_Virtual_Appliance_PI-6.rst)

2. Setup Ubuntu instance with IPFS installed
 - (Optional if you want Linux but you have a Windows machine):
   - Open Powershell as Administrator and type `wsl --install`
   - Once you do this you will have to restart your machine and it will 15/20 minutes to start Ubuntu
   - Then to install IPFS you can follow this [guide](https://github.com/Seagate/cortx/tree/main/doc/integrations/ipfs)
   - Alternatively here are the steps I followed:
     - Install dependencies:
     ```bash
     sudo apt update
     sudo apt full-upgrade
     sudo apt install gcc git clang build-essential wget -y
     ```
     - Install golang:
     ```bash
     wget -c https://go.dev/dl/go1.17.10.linux-amd64.tar.gz
     sudo tar -C /usr/local -xzf go1.17.10.linux-amd64.tar.gz
     export PATH=$PATH:/usr/local/go/bin
     export PATH=$PATH:$(go env GOPATH)/bin
     export GOPATH=$(go env GOPATH)

     go version
     ```
     - Install IPFS:
     ```bash
     git clone https://github.com/ipfs/go-ipfs
     cd go-ipfs/
     export GO111MODULE=on
     go get github.com/ipfs/go-ds-s3/plugin@v0.8.0
     echo -e "\ns3ds github.com/ipfs/go-ds-s3/plugin 0" >> plugin/loader/preload_list
     make build
     go mod tidy
     make build
     make install
     ```
     - Initialize IPFS:
     ```bash
     ipfs init
     ```
     - Update IPFS config files to use CORTX instance:

       Update the config file to point to your CORTX instance (see example config below):

     ```bash
     cd ~/.ipfs/
     vi config

     "Datastore": {
     "StorageMax": "10GB",
     "StorageGCWatermark": 90,
     "GCPeriod": "1h",
     "Spec": {
       "mounts": [
         {
           "child": {
             "type": "s3ds",
             "region": "us-east-1",
             "bucket": "traffic-data",
             "rootDirectory": "date",
             "regionEndpoint": "http://192.168.1.16:31949",
             "accessKey": "sgiamadmin",
             "secretKey": "ldapadmin"
           },
           "mountpoint": "/blocks",
           "prefix": "flatfs.datastore",
           "type": "measure"
         },
     ```

      Update the datastore_spec file to point to your CORTX instance (see example config below):

     ```bash
     vi datastore_spec
     {"mounts":[{"bucket":"traffic-data","mountpoint":"/blocks","region":"us-east-1","rootDirectory":"date"},{"mountpoint":"/","path":"datastore","type":"levelds"}],"type":"mount"}
     ```
    

### 1. Dowload python script
 ```bash
 git clone https://github.com/Seagate/cortx.git
 cd cortx/doc/integrations/cortx-here-traffic-alerts
 ```
 
### 2. Configure your script to use your CORTX instance
 - Edit the environment variables in the python script:
 ```bash
 ACCESS_KEY = 'sgiamadmin'
 SECRET_ACCESS_KEY = 'ldapadmin'
 END_POINT_URL = 'http://192.168.1.16:31949' 
 ```
   **Note:** To get these details follow these steps below:
   1. Run this command in your CORTX instance to get your ACCESS abd SECRET_ACCESS keys
      ```bash
      cat ~/.aws/credentials
      ```
   1. Run this command inside your CORTX instance to get the endpoint ip address:
      ```bash
      ip a l | grep ens
      ```
      Then run this command and you want to take the port number that is beside port 80 from this output (shown in picture below):
      ```bash
      kubectl get svc | grep 'server-loadbal'
      ```
      ![portnum](https://user-images.githubusercontent.com/23244853/177014915-5ad3e347-9a0f-43f9-94a2-bb1c0d7f58a5.PNG)
   1. Then our endpoint-url will look like this:
      ```bash
      http://<endpoint ip address>:<port number>
      ```
 
### 3. Setup cron job to run script at your desired time
 - If you use Windows you can go to `Control Panel` -> `Administrative Tools` -> `Task Scheduler` -> `Actions` -> `Create Task` -> `Actions` -> `New` -> `Browse` and then select your script.
  - On Mac/Linux 
  ```bash
  crontab -e
  ```
  - You want this script to run ideally before you leave so that you can avoid routes that have crashes on them
  - To set the time use [crontab.guru](https://crontab.guru/) to make sure you have set your cron job for the correct time
  - Then paste this output to your crontab file
 
 **e.g.**  00 9 * * * python here_cortx_traffic_data.py
 
### 4. Then once you build up a large dataset you can use this data to notify your local government about dangerous roads
  - Every day we append the data we got from the API to our dataset in CORTX
  
