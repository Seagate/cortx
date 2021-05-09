
# What is...

## cortx_dof?

Cortx_dof is a DoF integration into Seagate's CORTX server in Python. For managing data, it uses boto3. Cortx_dof helps to integrate PyTorch, TensorFlow and Elasticsearch into CORTX at the same time with the funcionality of DoF. The main functions of cortx_dof are:

- *uploading* with ` S3 `

- *searching* with ` Elasticsearch `

- *downloading* with ` S3 `


## CORTX?

[CORTX](https://www.seagate.com/products/storage/object-storage-software/) is 100% open source object storage system with features like mass capacity and great efficiency.


## DoF?

[DoF](https://github.com/hyperrixel/dof) is a file format and acronym for *Deep Model Core Output Framework*. It is a continuously developed hackathon winner project to provide fast dataset sharing and data-secure at the same time. It helps data scientists to handle sensitive data and to work with large datasets easily.


# How it works?

You can manage DoF files on CORTX with S3. DoF contains metadata to help you to mine the functionality of Elasticsearch better. DoF can contain different stages of the dataset such as ` raw data `, ` preprocessed data `, ` output of headless pretrained model `. Besides this, DoF can hold ` model data ` such as weights and biases. Furthermore, it is a container for any additional information like ` license `,  `contact of author `,  `description of model `, etc. There are a lot of prebuild key that help you to store the most important details about your dataset, model or training process. The list of keys are extendable.

## Video presentation

This video is about cortx_dof presentation and it is part of the submission.

[Project presentation video (submission video): https://youtu.be/w9zH1QEcw3w](https://youtu.be/w9zH1QEcw3w)

This video is a code walkthrough and it is **NOT** the part of the submission.

[Code walkthrough video (outside submission): https://youtu.be/1HEI_MdM1X8](https://youtu.be/1HEI_MdM1X8)

##  How to use?

- At the moment you cannot install cortx_dof as a python module however it was developed in a way to do so in the future. It has a ` requirements.txt ` file to install requirements easily.

- First of all, you need to have a working CORTX server. It can be a VM or a cloud instance.

- Since cortx_dof uses dof, you should install it with the following command ` pip install dofpy `. This step is skippable if you have installed all of the dependencies via our ` requirements.txt `.

- Cortx_dof uses elasticsearch for searching, so you should install it with the following command ` pip install elasticsearch `. This step is skippable if you have installed all of the dependencies via our ` requirements.txt `.

- Cortx_dof uses boto3 for handling S3 buckets, so you should install it with the following command ` pip install boto3 `. This step is skippable if you have installed all of the dependencies via our ` requirements.txt `.

- There is a ` config.json ` file with the most important configuration keys. If you have a local VM or any other access to cortx you just have to update the data there.


## Examples

In this section we show how the main functions work. Examples are not in alphabetical order, but in the order as it can happen during a real-life usage. For more details, please check the ` example.py ` or the ` docstrings ` of the code.


### Search for DoF file

 ``` python
CortxDof.search(search_expression, index, just_find)
 ```
- *search_expression*: Parameter to be searched. If ` string ` is given, it is treated as an **Id** to get. If a ` tuple ` is given it is treated as a simple **key = value** search. If ` dict ` is given it is directly forwarded to **elasticsearch** as a query.

- *index*: Name of the index to get.

- *just_find*: Whether to return a bool with the result of the search or return information about the found object(s).

Real example for ` matching all `. It is useful when you want to test the code or any function, since it always returns with something.
 ``` python
result = cortxdof.search({"query": {"match_all": {}}}, just_find=False)
 ```


### Get a DoF file

 ``` python
CortxDof.get(search_expression, object_key, bucket, index)
 ```
- *search_expression*: Parameter to be searched. If ` string ` is given, it is treated as an **Id** to get. If a ` tuple ` is given it is treated as a simple **key = value** search. If ` dict ` is given it is directly forwarded to **elasticsearch** as a query. If ` None ` is given **S3** is used.

- *index*: Name of the index to get.

- *object_key*: The name (key) to get the object in s3 with.

- *bucket*: Name of the bucket to store.

Real example for getting DoF file with S3
 ``` python
result = cortxdof.get(object_key=s3_ids[0], bucket=TEST_BUCKET)
 ```


### Describe DoF files

 ``` python
CortxDof.describe(dof_object)
 ```
- *dof_object*: Path of a DoF file, or the DoF file's instance.


### Store a DoF file

 ``` python
CortxDof.store(dof_object, object_key, bucket, index, allow_overwrite)
 ```
- *dof_object*: Path of a DoF file, or the DoF file's instance.

- *object_key*: The name (key) to store the object in s3 with. If empty string is given, a new random name is generated.

- *bucket*: Name of the bucket to store. If empty string is given, bucket name is retrieved from the configuration.

- *index*: Name of the index to store.

- *allow_overwrite*: Whether or not to allow to overwrite existing object.

Real example for storing a DoF file in a test bucket.
 ``` python
es_id = cortxdof.store(dofdata, bucket=TEST_BUCKET)
 ```

### Real lie example

Create ContainerInfo

``` python
info_core = ContainerInfo()
info_core['author'] = 'Test Author'
info_core['author_contact'] = 'email@example.com'
info_core['source'] = 'https://pytorch.org/vision/stable/datasets.html#mnist'
info_core['license'] = 'Creative Commons Attribution-Share Alike 3.0'
info_core['dataset'] = 'raw dataset'
 ```

Create DoF File

 ``` python
dof_raw = DofFile(info_core)
  ```

Add data into DoF
NOTE: raw_loader means in this case a PyTorch DataLoader

``` python
  for i, (x, y) in enumerate(raw_loader):
      dof_x = DataElement(x, DataElement.X)
      dof_y = DataElement(y, DataElement.Y)
      id_x = dof_raw.dataset.add_element(dof_x)
      id_y = dof_raw.dataset.add_element(dof_y)
      id_x = dof_raw.dataset.linker.link(id_x, id_y)
      if i >= REAL_COUNT:
          break
```

# Why should you used cortx_dof?

CORTX is an open source solution. Cortx_dof provides functionalities of DoF extended with features of CORTX. It is good for you if you use mass datasets or headless pretrained models, or you like to save each step of your deep learning development process. DoF can save space and training time for you.
