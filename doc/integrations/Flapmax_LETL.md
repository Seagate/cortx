# Overview

LETL ( Lightweight-ETL ) is an ETL framework created to provide configurable ETL services while offering analytical capabilities. This system aims to empower users and businesses to manage and control their data.


# The Mission

Big Data poses a challenge to organizations over a variety of different fields with respect to data management and storage. In particular, unstructured data (images, text, audio) is one of the fastest growing forms of data and is especially difficult to address. These artifacts may contain valuable insights but often go to waste as organizations are unable to manage them.

# Enter: LETL 

The LETL system is able to address the challenges of data movement and management in a fully managed system from end-to-end pipeline. This is achieved by leveraging technology conducive to latency-sensitive tasks such as hardware like SCM and PMem devices while also performing formatting tasks to make data compatible for destination systems. Data may also stay in the LETL ecosystem to utilize the underlying technology.

![The LETL Architecture with CORTX Integration](https://github.com/flapmx/cortx/tree/main/doc/integrations/Flapmax_LETL "The LETL Architecture with CORTX Integration")

The underlying technology and pipeline-like flow gives the LETL system the necessary latency, data throughput, and configurability to provide knowledge workers the performance needed for their specific analysis needs. An exemplary use case for this system is in the healthcare field for future events similar to Covid-19. Datasets were not only continually produced from many different sources and formats but also in large quantities as well. Consider that <size of single file instance; it’s big>

# CORTX-LETL Integration

Considering the previous technological architecture of LETL, CORTX can be integrated to provide data tiering between HDD’s, SSD’s, and PMEM. This provides flexibility for users and organizations for their own data needs.

# Instructions

[Follow along with our brief demonstration](https://www.youtube.com/watch?v=5qap5aO4i9A)

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


# Confirming Functionality

Data movement from CORTX into DAOS can be demonstrated by deleting Data A locally and reading Data A from DAOS

### /tiering/D-C
```
# Request Body
{
"destination_uri": "Covid-19/DNA/NC_045512/A1",
"source_uri": "public/tiering/A1",
"request_id": "temp_id5"
}
```
