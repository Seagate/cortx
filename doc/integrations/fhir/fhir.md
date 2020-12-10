# CORTX-FHIR Integration

![alt text](https://hl7.org/FHIR/assets/images/fhir-logo-www.png)
![alt text](https://www.seagate.com/www-content/product-content/storage/object-storage-software/_shared/images/enterprise-cortex-pdp-row1-content-image.png)

**What is FHIR?**
FHIR (Fast Healthcare Interoperability Resources) is a standard describing data formats, elements and an application programming interface for exchanging electronic health records.\
The standard was created by the Health Level Seven International healthcare standards organization,\
and it is widely used in healthcare industry.

FHIR has also been implemented by two of the largest EHR (Electronic Health Record) companies in the US: Epic and Cerner.

More data can be found here: https://www.hl7.org/fhir/ \
**Note:** Latest FHIR version is **4.0.1**, but many organizations are still working with version **3.0.2 (STU 3)**.

**What is CORTX?**
CORTX is a distributed object storage system designed for great efficiency, massive capacity, and high HDD-utilization. CORTX is 100% Open Source.

**Why this integration important?**
As known, hospitals are mostly On-Prem for privacy and security reasons.\
Therefore, combining CORTX abilities with hospitals' big data can improve data storage and data analysis solutions for clinical decision making.\
During COVID 19 time, those skills have been proven to be missing and the entire healthcare industry working on finding solutions.

**Concept pitch can be found here:**
https://www.loom.com/share/6b06558ddea14bca9518fe682af969d3


## Integration code walk-through



### Connectors
    - mapper + supported models: There are a lot of models supported by FHIR, we have not implemented all of them. In order to do so, you will have to edit this files.
    - connector: basic abstract connector
    - FHIR connector: handles all API requests from/to FHIR service
    - CORTX connector: handles all API requests from/to CORTX service

### Server
    A Flask-based server for transferring data from/to both FHIR based APIs and CORTX.
    Available functions on models: Get, Insert, Update, Delete, Search.

### What can be done next?
    - Add supported models
    - Allow multiple configurations loading to server.py using env files (instead of config.json file)

### Contributors
    - Amit Yaniv
    - Avi Greenwalds

### Open FHIR Servers
    - Servers can be found here: https://wiki.hl7.org/Publicly_Available_FHIR_Servers_for_testing
    - Servers status can be found here: https://stats.uptimerobot.com/9DArDu1Jo

    **We have tested our code mostly on Grahame's test server (r3 [STU 3] - the FHIR most used version today)**

### Example
    An example for reading data from open FHIR API service and writing it to CORTX.

    The simplest configuration needed for FHIR connector is just a server api base url.
    However, if the server is using OAuth2 authentication, you must also provide an app id, api key and secret.
    CORTX S3 connector requires an endpoint url, AWS access key id and AWS secret access key.

    Most of the open FHIR servers allow only read (get) and search, as they are read-only.
    On our example, we transfer data from a FHIR server to CORTX, update the data on CORTX, and validate the change.

### Demo
    Demo video can be found here: https://www.loom.com/share/c7bdb03c92844b56a8fdd56ff7c1a5cc