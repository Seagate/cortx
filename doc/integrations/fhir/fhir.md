# CORTX-FHIR Integration

![alt text](https://hl7.org/FHIR/assets/images/fhir-logo-www.png)
![alt text](https://www.seagate.com/www-content/product-content/storage/object-storage-software/_shared/images/enterprise-cortex-pdp-row1-content-image.png)

**What is FHIR?**
FHIR (Fast Healthcare Interoperability Resources) is a standard describing data formats, elements and an application programming interface for exchanging electronic health records.\
The standard was created by the Health Level Seven International healthcare standards organization,\
and it is widely used in healthcare industry.

FHIR has also been implemented by two of the largest EHR (Electronic Health Record) companies in the US: Epic and Cerner.

More data can be found [here](https://www.hl7.org/fhir/).\
**Note:** Latest FHIR version is **4.0.1**, but many organizations are still working with version **3.0.2 (STU 3)**.

**What is CORTX?**
CORTX is a distributed object storage system designed for great efficiency, massive capacity, and high HDD-utilization. CORTX is 100% Open Source.

**Why this integration important?**
As known, hospitals are mostly On-Prem for privacy and security reasons.\
Therefore, combining CORTX abilities with hospitals' big data can improve data storage and data analysis solutions for clinical decision making.\
During COVID 19 time, those skills have been proven to be missing and the entire healthcare industry working on finding solutions.

**Concept pitch** can be found [here](https://www.loom.com/share/6b06558ddea14bca9518fe682af969d3).


## Integration code walk-through
    Our sample of integration can be found [here](fhir/example.py).
    **Note:**
    FHIR have a lot of models, we have only implemented four models.
    Supported model enum can be found [here](fhir/connectors/supported_models.py).

    ### Step 1 - Download requirements
        We used a FHIR client to communicate with FHIR based server and boto3 for CORTX S3.
        For our generic server we used Flask (a web framework).
        Flask documentation can be found [here](https://flask.palletsprojects.com/en/1.1.x/tutorial/layout/)

        pip install boto3 fhirclient flask

    ### Step 2 - Supported models
        FHIR have a lot of models, we have only implemented four models.
        Supported model enum can be found [here](fhir/connectors/supported_models.py).
        To make the code generic as possible we also needed to create [mappers](fhir/connectors/mapper.py):
        - get_fhir_model_type: map between a supported model (enum) to fhirclient's model-type
        - get_cortx_s3_bucket: map between a supported model (enum) to CORTX S3 bucket name
        - revised_mapping: map between a string name of model to supported model (enum).

        Those mappers are being used inside our connectors and flask server.

    ### Step 3 - Understand connector classes
        Connector class in an abstract class for handling data manipulations (CRUD - Create, Read, Update, Delete).
        FhirConnector and CortxConnector have inherited this class and implemented its methods.
        The possible methods for a model (supported models are in supported_models.py enum as described above) are:
        - connect: connect to the server (FHIR server / CORTX S3)
        - get: get a model by id
        - insert: insert a new model
        - update: update a new model
        - delete: delete a model by id
        - search: search models' data by given search terms, supported term (per model) can be found [here](http://test.fhir.org/r3).

        **Note:**
        search function won't work on CortxConnector as S3 buckets are read by id only.

    ### Step 4 - Create a FHIR connector
        FhirConnector is a class that only requires api_base (FHIR api base url) in its constructor.
        If the FHIR server also required authentication you should also send app_id, api_key (OAuth2 access key) and secret (OAuth2 secret).
        After initiating the class call 'connect' method, it will handle authentication if needed.

        **Code:**
            fhir_connector = FhirConnector('http://test.fhir.org/r3')
            fhir_connector.connect()

    ### Step 5 - Create a CORTX connector
        CortxConnector is a class that requires endpoint_url (CORTX url), aws_access_key_id (AWS access key), aws_secret_access_key (AWS access secret) in its constructor.
        After initiating the class call 'connect' method, it will handle authentication.

        **Code:**
            cortx_connector = CortxS3Connector(endpoint_url='http://uvo19iqqprct5bd9622.vm.cld.sr',
                                               aws_access_key_id='z5_sdXvsSvKKWxjJmvCA9g',
                                               aws_secret_access_key='kUIMmfQCyNzxel8vi8udIsadwuliOYpS/jdyMh8e')
            cortx_connector.connect()

    ### Step 6 - Transfer data from FHIR server to CORTX S3
        Get a patient by id from FHIR, insert it to CORTX.

        **Code:**
            patient = fhir_connector.get(SupportedModels.PATIENT, '11')
            insert_response = cortx_connector.insert(SupportedModels.PATIENT, patient.as_json())

## Demo
    A quick (1 minute) demo video can be found [here](https://www.loom.com/share/e75d68b1bdb441a2bd450ae5501c3e74).
    A Full (8 minute) demo video which describe the entire code can be found [here](https://www.loom.com/share/c7bdb03c92844b56a8fdd56ff7c1a5cc).

## Flask server walk-through
    Flask is a Python web framework used for developing web applications.
    Flask documentation can be found [here](https://flask.palletsprojects.com/en/1.1.x/tutorial/layout/)

    We used this server for enabling an easier way of transferring data from/to both FHIR-based servers and CORTX S3.

    ### Step 1 - Configuration
        We have created a [configuration file](fhir/server/config.json) from which the server reads its configuration.
        Configuration contains:
        - FHIR host (base url of FHIR API server)
        - CORTX endpoint url, AWS access key and AWS access secret

        We load the configuration by using json.load function.
        **Code:**
            with open('config.json') as json_data_file:
                config = json.load(json_data_file)

    ### Step 2 - Initialize connectors and app
        As described in the integration walk-through, we initialize our connectors using the configuration.

        **Code:**
            fhir_connector = FhirConnector(config['fhir']['host'])
            fhir_connector.connect()

            cortx_connector = CortxS3Connector(endpoint_url=config['cortx']['endpoint_url'],
                                   aws_access_key_id=config['cortx']['aws_access_key_id'],
                                   aws_secret_access_key=config['cortx']['aws_secret_access_key'])
            cortx_connector.connect()

        Then, we initialize an app using file name.

        **Code:**
            app = Flask(__name__)

    ### Step 3 - Understand private methods
        We used three private methods for our server:
        - __get_connector: returns the relevant connector according to the given source ('CORTX','FHIR')
        - __get_supported_model: return supported model (enum) according to the given supported model's name (string)
        - __parse_response: parse the response data (on get, search methods) according to the relevant data source ('CORTX','FHIR').

    ### Step 4 - Understand flask api
        Flask is using 'app.route' decorators for defining urls.
        'app' is the Flask instance described in step 2.

        **Example:**
            @app.route('/<source>/<supported_model_name>/<model_id>', methods=['GET'])
            def get_model(source: str, supported_model_name: str, model_id: str):
                ...

        On that example, flask is expecting to get three parameters:
        - source ('CORTX','FHIR')
        - supported_model_name ('patient','observation','procedure','appointment')
        - model_id (model id to get from FHIR/CORTX server)

        The 'methods' argument (methods=['GET']) defines the HTTP methods allowed for this function.

    ### Step 5 - Server methods
        All of the methods contains two common parameters:
        - source (str): we will use it to get relevant connector and parse data.
        - supported_model_name (str): we will use it to get FHIR client's model or CORTX S3 bucket.

        Methods:
        - get_model - get a model by id
        - insert_model - insert a model (by given json on HTTP POST method)
        - update_model - update a model (by given json on HTTP PUT method)
        - delete_model - delete a model by id
        - search_model - search models (by given json-terms on HTTP POST method)

    ### Step 6 - Run the Flask server
        **Code:**
            if __name__ == '__main__':
                app.run()

        And we are up and running :)

## Open FHIR Servers
    - Servers can be found [here](https://wiki.hl7.org/Publicly_Available_FHIR_Servers_for_testing)
    - Servers status (up/down times) can be found [here](https://stats.uptimerobot.com/9DArDu1Jo)

    **Note:**
    We have tested our code mostly on Grahame's test server (r3 [STU 3] - the FHIR most used version today).

    **Note 2:**
    Most of the open FHIR servers allow only read (get) and search, as they are read-only.

## What's next?
    - Add more supported models
    - Allow multiple configurations loading to server.py using env files (instead of config.json file)

## Contributors
    - Amit Yaniv
    - Avi Greenwalds