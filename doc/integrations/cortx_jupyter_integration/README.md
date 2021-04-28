# [Cortx Jupyter Integration](https://devpost.com/software/cortx-jupyter)

### Jupyter Notebook Integration for Cortx Object Storage.

**Built for [Seagate Cortx Hackathon 2021](https://devpost.com/software/cortx-jupyter)**

## What is it?

[Cortx Jupyter Integration](https://devpost.com/software/cortx-jupyter) is an Open Source python package which combines the power of Cortx and Jupyter Notebooks to empower you to save and manage all of your Notebooks, Checkpoints and Data Files on Cortx. As a bonus, you can also use Cortx Jupyter Integration to load any pre-trained models from or to cortx server without any hassle of configuring or using Library specific integrations.

![logo](/media/cortx_jupyter_header.png)

## Description

Jupyter is widely used by all Data Scientists, Researchers, Students and Engineers across the Software Industry to perform all types of Data Science tasks. Cortx is a high performance and scalable Object Storage. And we think that it is a match made in heaven to bring them together!

Haven't you had those scary moments when your disk crashes after you finished training a Machine Learning model on Jupyter Notebook after hours of work. No more losing precious work because you forgot to save changes or no more worrying about local filesystem crashes or paying exorbitant subscription fees for Premium features of Hosted Jupyter Notebooks. **Cortx Jupyter Integration** is here to save you from all these! **Cortx Jupyter** is an Open Source python package which combines the power of Cortx and Jupyter Notebooks to empower you to store all of your Jupyter Notebooks, Checkpoints and Data Files on **Cortx Object Storage** instead of Jupyter's standard filesystem-backed storage.

When you opt to use a plain Jupyter notebook as your development environment, everything is saved in your local machine. If you want your Jupyter notebooks to be accessible to you from anywhere or any device, then *Cortx Jupyter Integration*  is the way to go. All of your Jupyter notebooks, checkpoints and data files are saved in your *Cortx Object Storage*, so that you can access it from anywhere on the go.  

*Cortx Jupyter Integration* can be used by developers and organizations who want a central repository of Notebooks, Checkpoints and Files. This feature can help multiple developers across an organization to collaborate with each other. *Cortx Jupyter Integration* integration periodically saves updates to your notebook as checkpoints to *Cortx Object Storage* so that you can either revert to a previous checkpoint or your colleague can continue working on the Jupyter Notebook from where you left. 

You don't have to worry about having notebooks and data saved in different places. With **Cortx Jupyter Integration**, you can have them together on **CORTX: World's Only 100% Open Source Mass-Capacity Optimized Object Store**. Now, you can concentrate on Machine Learning while *Cortx Jupyter* does the boring work of saving and tracking your work.

You can use Cortx Jupyter Integration to run any Jupyter Notebook that uses complex Machine Learning or Deep Learning models without dependency on the Library's native configuration files. ie., When you use `Cortx Jupyter Integration`, there is no need for changing configuration files or worrying about library native way of fetching data or using another ML Library specific integration to build ML models such as Pytorch, Tensorflow, Keras, etc. `Cortx Jupyter Integration` provides an easy way to Load data from Cortx into your Jupyter notebook  and pass it to any Machine Learning library. *As we promised before, you only have to worry about Machine Learning, we worry about Jupyter and Cortx integration!*
 
You can find the Python Package that we built at [pypi.org](https://pypi.org/project/cortx-jupyter/)

## Architecture

#### The integration in implemented in python 3, and consists of the following parts:

- `cortx_authenticator.py`: Class that provides functionality to authenticate to Cortx.
- `cortx_file_manager.py`: This Module provides methods that can be used by anyone importing cortx_jupyter Python package to use methods such as `read_data()`, `write_data()`, `read_model()` and `write_model()`.
- `cortx_jupyter.py`: Class that provides core functionality of integrating Jupyter Notebook to Cortx. Jupyter Notebook calls the methods in this class to perform file operations instead of it's in-built File Manager.
- `utils.py`: This class all the additional functionality such as using boto3 to send requests to Cortx.

![architecture](/media/cortx_jupyer_architecture.png)


## Features
 
 - Seamlessly Save notebooks, checkpoints, data files to Cortx.
 - Save multiple checkpoints for each notebooks to Cortx.
 - Checkpoints are saved to Cortx, under the key `<file_name>/.checkpoints/`. 
 - Restore from any of the previous checkpoints.
 - Multiple checkpoints are saved.
 - Already, have notebooks on S3? No worries, **Cortx Jupyter integration** can help you can switch easily from S3 to Cortx Open Source object storage.
 - Use **Cortx Jupyter integration's** `read_data()` and `write_data()` APIs to Read and Write large amount of data to and from your notebook directly from Cortx High Performance Object Storage for Machine Learning tasks.
 - Use **Cortx Jupyter integration's** `read_model()` and `write_model()` APIs to Save and Load a pre-trained model Machine learning models from Cortx High Performance Object Storage for Machine Learning tasks.
 - Delete Notebooks, Files that you don't need from Cortx.
 - Renaming Notebook name automatically updates Notebook and Checkpoint names on Cortx.
 - Jupyter Notebook is not blocked when requests are made to Cortx as everything has been implemented asynchronously.
 - View, Upload and Download any types of files that are in Cortx using Jupyter
  

## Prerequisites

###  Setup Cortx

Use the instructions at the following link to setup CORTX:

https://github.com/Seagate/cortx/blob/main/QUICK_START.md


## Setup Instructions

[Setup Instructions Video](https://youtu.be/GGAUWTDkhp8)

### 1. Installation

Install the Cortx Jupyter python package using the following command:

    pip install cortx-jupyter

You can find the package on [pypi.org](https://pypi.org/project/cortx-jupyter/)

### 2. Add Jupyter Config

Configure Jupyter to our `Cortx Jupyter`integration for its storage backend. This can be done by modifying your notebook config file. On a Unix-like system, your Jupyter Notebook config will be located at `~/.jupyter/jupyter_notebook_config.py`

**NOTE:** If you can't find this config file on your machine, you can create this file using the following command in terminal:

    jupyter notebook --generate-config

Now, edit the `~/.jupyter/jupyter_notebook_config.py`  file. 

**NOTE:** Please remember to replace credentials(`access_key_id` , `secret_access_key`) and `endpoint_url` with credentials of your Cortx environment.


    import cortx_jupyter
    from cortx_jupyter import CortxJupyter, CortxAuthenticator
    
    c = get_config()
    
    c.NotebookApp.contents_manager_class = CortxJupyter
    c.CortxJupyter.authentication_class = CortxAuthenticator
    
    
    c.CortxAuthenticator.access_key_id = "YOUR_ACCESS_KEY_ID"
    c.CortxAuthenticator.secret_access_key = "YOUR_SECRET_ACCESS_KEY"
    c.CortxJupyter.endpoint_url = "http://uvo1ettj69aisne19p9.vm.cld.sr"
    c.CortxJupyter.bucket_name = "testbucket"
    c.CortxJupyter.prefix = "notebooks/test/"


**Following Configuration options are available on CortxAuthenticator:**

`access_key_id` *(required)* 

`secret_access_key` *(required)* 

**Following Configuration options are available on CortxJupyter:**

`endpoint_url`*(required)* - Endpoint URL of your Cortx instance.

Example: ```http://uvo1ettj69aisne19p9.vm.cld.sr```

`bucket_name`*(required)*  - Cortx Bucket Name where you want to store your notebook.

Example: ```testbucket```

`prefix`*(required)*  - Path in the bucket where you want to store your notebook.

Example: ```notebooks/test/```

### 3. Test if it works

Now that we have completed installation and configuration, it is time to run Jupyter Notebook and check if the Jupyter Cortx magic works!

Use the following command on Linux-like systems to run Jupyter Notebook server:

    jupyter notebook

This will print some information about the notebook server in your terminal(as seen in the screenshot below), including the URL of the web application (by default,  `http://localhost:8888`):

![Jupyter Notebook Run](/media/jupyter_run.png)

It will then open your default web browser to this URL(as seen in the screenshot below). When the notebook opens in your browser, you will see the Notebook Dashboard, which will show a list of the notebooks, files, and subdirectories present in Cortx.

![Jupyter Cortx Files](/media/jupyter_cortx_files.png)

If you go to Cyberduck and list the files present in Cortx(as seen in the screenshot below), you can verify that all your Notebooks, Checkpoints and Data Files are stored on Cortx by the `Cortx Jupyter Integration`:

![Cyberduck](/media/cyberduck.jpeg)

If you want some examples to get started on Jupyter Notebooks with Cortx as storage, take a look at the following sample notebooks in our repository:

 - [Tensorflow example](/Examples/Tensorflow-Demo.ipynb)
 - [Pytorch example](/Examples/Pytorch-Demo.ipynb)
 - [Keras example](/Examples/Keras-Demo.ipynb)

## Use any Machine Learning library to train models on data stored in Cortx

When you use `Cortx Jupyter Integration`, there is no need for changing configuration files or worrying about library native way of fetching data. Load data from Cortx into your Jupyter notebook using `Cortx Jupyter Integration` an pass it to any Machine Learning library. *As we promised before, you only have to worry about Machine Learning, we worry about Jupyter and Cortx integration!*

Import Cortx Jupyter Python Package's methods into your notebook using the following statements:

    from cortx_jupyter import read_data, write_data, read_model, write_model

Following 4 methods are available to work work with data when using Cortx and Jupter Notebook.

|API method| Description |Parameters|
|--|--|--|
| **read_data()**  | Reads any type of data from Cortx into a variable in Python that you can pass as input to different libraries. |  file_name |
| **write_data()**  | Writes any type of data to Cortx. | file_name, data |
| **read_mode()**  | Reads a trained Machine Learning model from Cortx. | file_name, model |
| **write_model()**  | Writes a trained Machine Learning model to Cortx. | file_name, model |
 
 

 **read_data() Example:**
 
    from cortx_jupyter import read_data, write_data
    
    import pandas as pd
    
    data = read_data('ionosphere.csv')
	df = pd.read_csv(data, header=None)                                                                                                                          

In above example, take a look at how we load data from Cortx using `Cortx Jupyter Integration` . You can pass it to different ML libraries. **You can load data from Cortx to Jupyter to work with Keras, Tensorflow, Pytorch, etc using the following method. There is absolutely no dependency on the ML library that you are working with.** (thanks to `Cortx Jupyter Integration`)!

 **write_data() Example:**
 
    from cortx_jupyter import read_data, write_data
    
    import pandas as pd
    
    data = write_data('filename.csv', df)


If you want more examples, take a look at the following sample notebooks:

 - [Tensorflow example](/Examples/Tensorflow-Demo.ipynb)
 - [Pytorch example](/Examples/Pytorch-Demo.ipynb)
 - [Keras example](/Examples/Keras-Demo.ipynb)

The screenshot below shows that you can load data from Cortx and pass it to any ML library or Write data to Cortx using simple methods provided in our Cortx Jupyter Python Package:

![Read and Write](/media/read_write_cortx_jupyter.png)


## Revert Checkpoints

![Checkpoints](/media/revert_checkpoint.png)


## What we learned & Accomplishments that we're proud of

This was our first time publishing a Python Package to Python Package Index. It was quite an inspiration to learn how Jupyter works internally by looking at all the complex and well-written source code and writing code to integrate it with Cortx Object Storage. Also, this was our first time contributing to an open source project which makes us even prouder to have contributed to Open-Source software. We also learnt asynchronous networking library called `tornado` to make our integration's network IO non-blocking. This Hackathon has made us more interested in contributing to more Open Source projects.

## Challenges we ran into

When there was delay in fetching objects from Cortx storage due to networking issues, it was causing Jupyter Notebook to be blocked. We had to learn and use an asynchronous networking library called `tornado` to make sure that any issue with network IO blocking doesn't hamper User Experience. We were able to successfully resolve the issue and make the User Experience better with this. Also, given the time constraint and the size of the Jupyter project, it was a tough task for us to go through all of Jupyter's Source code and understand and identify the methods that we may have to rewrite to ensure that Jupyter uses Cortx for all of its storage functionality. However, we loved the experience of looking into source code of a product that we use in our daily professional lives and extending its functionality to work with an High Performance and Scalable Object Storage like Cortx.

## How we built it?

 - Cortx
 - S3 API
 - Python
 - Python Package Index
 - Jupyter
 - boto3
 - Pandas
 - Keras
 - tornado
 - numpy
 - joblib
 - tensorflow
 
## Demo Video

Watch the [Youtube video](https://youtu.be/TFZvOT2fbXw) to learn more about the project.

[Complete Information](https://youtu.be/cgLWvrlB75Q)

[Features Demo](https://youtu.be/TFZvOT2fbXw)

[Setup Instructions Video](https://youtu.be/GGAUWTDkhp8)

## What's next for Cortx Jupyter Integration

- We aim to provide an option to switch between S3, Cortx and FileSystem.
- Provide more methods in cortx_jupyter package.

## Contributors:

[Sumanth Reddy Muni](https://www.linkedin.com/in/sumanthmuni/)

[Priyadarshini Murugan](https://www.linkedin.com/in/priya-murugan/)
