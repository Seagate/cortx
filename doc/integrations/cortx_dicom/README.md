# What is...

## cortx_dicom?

Cortx_dicom is a dicom integration into Seagate's CORTX server in Python. Handling dicom files is based on pydicom. It has 5 major functionalities:

- collecting metadata [ ` cortx_dicom ` and ` pydicom ` ]
- managing data [ ` S3 ` ]
- searching [ ` Elasticsearch ` ]
- warning about potential legal and data risks [ ` cortx_dicom ` ]
- removing protected health information from dicom [ ` pydicom ` ]

We already integrated **4840 dicom tags** into our system as the base of the labeling method. For the legal parts, we implemented the warning features for **256 potentially risky tags**.


## CORTX?

[CORTX](https://www.seagate.com/products/storage/object-storage-software/) is 100% open source object storage system with features like mass capacity and great efficiency.


## dicom?

[Dicom](https://en.wikipedia.org/wiki/DICOM) is a widely used standard in healthcare to store and transmit medical imaging information and related data.


# How it works?

Cortx_dicom can be used locally with a CORTX-VirtualMachine (VM) or remotely with a cloud-based server.

## Video presentation

This video is about cortx_dicom presentation and it is part of the submission.

[Project presentation video (submission video): https://youtu.be/Qxb3cFnwXnQ](https://youtu.be/Qxb3cFnwXnQ)


This video is a code walkthrough and it is **NOT** the part of the submission.

[Code walkthrough video (outside submission): https://youtu.be/emRk4xqBDmQ](https://youtu.be/emRk4xqBDmQ)

##  How to use?

- At the moment you cannot install cortx_dicom as a python module however it was developed in a way to do so in the future. It has a ` requirements.txt ` file to install requirements easily.

- First of all, you need to have a working CORTX server. It can be a VM or a cloud instance.

- Since cortx_dicom uses pydicom, you should install it with the following command ` pip install pydicom ` or if you are an Anaconda user, feel free to use the ` conda install -c conda-forge pydicom ` command. This step is skippable if you have installed all of the dependencies via our ` requirements.txt `.

- Cortx_dicom uses elasticsearch for searching, so you should install it with the following command ` pip install elasticsearch `. This step is skippable if you have installed all of the dependencies via our ` requirements.txt `.

- Cortx_dicom uses boto3 for handling S3 buckets, so you should install it with the following command ` pip install boto3 `. This step is skippable if you have installed all of the dependencies via our ` requirements.txt `.

- There is a ` config.json ` file with the most important configuration keys. If you have a local VM or any other access to cortx you just have to update the data there.


## Examples

In this section we show how the main functions work. Examples are not in alphabetical order, but in the order as it can happen during a real-life usage. For testing purposes, we provide some test dicom files. For more details, please check the ` example.py ` or the ` docstrings ` of the code. The source of dicom files is: https://github.com/datalad/example-dicom-structural.


### Search for dicom file

 ``` python
CortxDicom.search(search_expression, index, just_find)
 ```
- *search_expression*: Parameter to be searched. If ` string ` is given, it is treated as an **Id** to get. If a ` tuple ` is given it is treated as a simple **key = value** search. If ` dict ` is given it is directly forwarded to **elasticsearch** as a query.

- *index*: Name of the index to get.

- *just_find*: Whether to return a bool with the result of the search or return information about the found object(s).

Real example for ` matching all `. It is useful when you want to test the code or any function, since it always returns with something.
 ``` python
result = cortxdicom.search({"query": {"match_all": {}}}, just_find=False)
 ```


### Get a dicom file

 ``` python
CortxDicom.get(search_expression, object_key, bucket, index)
 ```
- *search_expression*: Parameter to be searched. If ` string ` is given, it is treated as an **Id** to get. If a ` tuple ` is given it is treated as a simple **key = value** search. If ` dict ` is given it is directly forwarded to **elasticsearch** as a query. If ` None ` is given **S3** is used.

- *index*: Name of the index to get.

- *object_key*: The name (key) to get the object in s3 with.

- *bucket*: Name of the bucket to store.

Real example for getting dicom file with S3
 ``` python
result = cortxdicom.get(object_key=s3_ids[0], bucket=TEST_BUCKET)
 ```


### Describe dicom files

 ``` python
CortxDicom.describe(dicom_object, key_scheme)
 ```
- *dicom_object*: Path of a DICOM file, or the DICOM file's instance.

- *key_scheme*: Scheme of DICOM keys to add. Available schemes: ` KEY_SCHEME_HUMAN_READABLE `, ` KEY_SCHEME_NUMERIC `, ` KEY_SCHEME_SPACELESS `

Real example for printing the content of a dicom file in a human readable format.
 ``` python
print(CortxDicom.describe(dicomfile, CortxDicom.KEY_SCHEME_HUMAN_READABLE))
 ```


### Using filters

 ``` python
CortxDicom.apply_filter(dicom_object, filter_type)
 ```
- *dicom_object*: Path of a DICOM file, or the DICOM file's instance.

- *filter_type*: Filters to apply. Available filters: ` FILTER_NO_FILTER `, ` FILTER_PRIVATE `, ` FILTER_HIPAA `, ` FILTER_GDPR `

Real example for applyling HIPAA filter on a dicom file. WARNING: Deleting from a pydicom instance usually mean inplace operation. This means it is not necessarily to use the returned variable.
 ``` python
CortxDicom.apply_filter(dicomfile, CortxDicom.FILTER_HIPAA)
 ```

### Store a dicom file

 ``` python
CortxDicom.store(dicom_object, object_key, bucket, index, allow_overwrite)
 ```
- *dicom_object*: Path of a DICOM file, or the DICOM file's instance.

- *object_key*: The name (key) to store the object in s3 with. If empty string is given, a new random name is generated.

- *bucket*: Name of the bucket to store. If empty string is given, bucket name is retrieved from the configuration.

- *index*: Name of the index to store.

- *allow_overwrite*: Whether or not to allow to overwrite existing object.

Real example for storing a dicom file in a test bucket.
 ``` python
es_id = cortxdicom.store(dicomfile, bucket=TEST_BUCKET)
 ```


# Why should you used cortx_dicom?

CORTX is an open source solution. Cortx_dicom can help you to handle dicom files on a legally proper way to meet the requirements of different regulations like **HIPAA**, **GDPR** or **CCPA**.
