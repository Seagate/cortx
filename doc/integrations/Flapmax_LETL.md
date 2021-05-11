## Optimizing ETL Pipelines with CORTX and Flapmax Light-weight ETL Engine

LETL ( Lightweight-ETL ) is an ETL framework created to provide configurable ETL services while offering fast analytics capabilities. This system aims to empower users and businesses to manage and control their data.


## The Mission

Big Data poses a challenge to organizations over a variety of different fields with respect to data management and storage. In particular, unstructured data (images, text, audio) is one of the fastest growing forms of data and is especially difficult to address. These artifacts may contain valuable insights but often go to waste as organizations are unable to manage them.

## Enter: LETL 

The LETL system is able to address the challenges of data movement and management in a fully managed system end-to-end pipeline. This is achieved by leveraging technology conducive to latency-sensitive tasks such as hardware like SCM and PMem devices while also performing formatting tasks to make data compatible for destination systems. Data may also stay in the LETL ecosystem to utilize the underlying technology.

![diagram](https://user-images.githubusercontent.com/75850728/116353584-e0eff300-a7ab-11eb-9b21-ca78a9e30c82.png)

The underlying technology and pipeline-like flow gives the LETL system the necessary latency, data throughput, and configurability to provide knowledge workers the performance needed for their specific analysis needs. An exemplary use case for this system is in the healthcare field for future events similar to Covid-19. Datasets were not only continually produced from many different sources and formats but also in large quantities as well. 

Consider the overwhelming amount of data in fields such as drug/vaccine discovery, precision medicine & genomics, smart hospital and multi-omics data analysis, health informatics, and you can start to visualize the real-world impact of harnessing it for insights.

## CORTX-LETL Integration

Considering the previous technological architecture of LETL, CORTX can be integrated to provide data tiering between HDD’s, SSD’s, and PMEM. This provides flexibility for users and organizations for their own data needs.

## Instructions

[Follow along with our brief demonstration](https://youtu.be/Nz05Z_gcNzI)

We will walk you through our FastAPI and you can follow along as we demonstrate the LETL-CORTX integration. Let's go endpoint by endpoint.

### 1. /cortx/write: Write data A into CORTX

```
# Request body
{
"destination_uri": "Covid-19/DNA/NC_045512",
"source_uri": "project-19-276121/letlCovidSet/A1",
"request_id": "temp_id"
}
```
### 2. /daos/ssd/write: Write data B into DAOS

```
# Request Body
{
"destination_uri": "daos/public/data",
"source_uri": "project-19-276121/letlCovidSet/B1",
"request_id": "temp_id2"
}
```
### 3. /tiering/D-C : Move data B from DAOS into CORTX
```
# Request Body
{
"destination_uri": "public/tiering/B1",
"source_uri": "daos/public/data/B1",
"request_id": "temp_id3"
}
```
### 4. /tiering/C-D : Move data A from CORTX into DAOS

  
```
# Request Body
{
"destination_uri": "public/tiering/A1",
"source": "Covid-19/DNA/NC_045512/A1",
"request_id": "temp_id4"
}
```
  

## API Requirements for writing to DAOS or CORTX

### 1.) destination_uri must be unique.

### 2.) request_id must be unique.

### 3.) source_uri must be either:
`“project-19-276121/letlCovidSet/A1”`
or
`“project-19-276121/letlCovidSet/B1"`


## Confirming Functionality

Data movement from CORTX into DAOS can be demonstrated by viewing the data on the VMs.

### We start with dataset B1 inside of DAOS.

![MicrosoftTeams-image (4)](https://user-images.githubusercontent.com/75850728/116342075-daa34c00-a796-11eb-971b-169791db0672.png)

### When tiering from DAOS to CORTX, dataset B1 appears on CORTX.

![MicrosoftTeams-image (3)](https://user-images.githubusercontent.com/75850728/116342147-f9094780-a796-11eb-9ea4-ac05713b90fd.png)

### And finally, we tier from CORTX to DAOS by transferring dataset A1.

![MicrosoftTeams-image (5)](https://user-images.githubusercontent.com/75850728/116342204-13dbbc00-a797-11eb-9eea-8ca86dba6456.png)

