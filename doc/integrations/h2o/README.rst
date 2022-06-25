##########################
H2O Integration with CORTX
##########################

`video link <https://vimeo.com/582061280>`__

H2O is an in-memory platform for distributed, scalable machine learning. H2O uses familiar interfaces like R, Python, Scala, Java, JSON and the Flow notebook/web interface, and works seamlessly with big data technologies like Hadoop and Spark. H2O provides implementations of many popular algorithms such as Generalized Linear Models (GLM), Gradient Boosting Machines (including XGBoost), Random Forests, Deep Neural Networks, Stacked Ensembles, Naive Bayes, Generalized Additive Models (GAM), Cox Proportional Hazards, K-Means, PCA, Word2Vec, as well as a fully automatic machine learning algorithm (H2O AutoML).

######
Step 1
######

Downloading & Installing H2O
============================

This section describes how to download and install the latest stable version of H2O. These instructions are also available on the `H2O Download page <http://h2o-release.s3.amazonaws.com/h2o/latest_stable.html>`__. 

We will only focus on the Python side of H2O in this README.

Install in Python
-----------------

Run the following commands in a Terminal window to install H2O for Python. 

1. Install dependencies (prepending with ``sudo`` if needed):

.. code-block:: bash

	pip install requests
	pip install tabulate
	pip install future

**Note**: These are the dependencies required to run H2O. A complete list of dependencies is maintained in the following `file <https://github.com/h2oai/h2o-3/blob/master/h2o-py/conda/h2o/meta.yaml>`__.

2. Run the following command to remove any existing H2O module for Python.

.. code-block:: bash

    pip uninstall h2o

3. Use ``pip`` to install this version of the H2O Python module.

.. code-block:: bash

	pip install -f http://h2o-release.s3.amazonaws.com/h2o/latest_stable_Py.html h2o

**Note**: When installing H2O from ``pip`` in OS X El Capitan, users must include the ``--user`` flag. For example:

.. code-block:: bash
	
   pip install -f http://h2o-release.s3.amazonaws.com/h2o/latest_stable_Py.html h2o --user

######
Step 2
######
   

Using H2O with CORTX
====================

The relevant code is found in ``h2o.ipynb``, with comments and guides to how to connect a H2O server with CORTX. 

The code can be deployed on `Google's Colaboratory <https://research.google.com/colaboratory/>`__ as well.


Most importantly, the code to ensure the connections is shown below

.. code-block:: python

    with open("core-site.xml","w") as f:
        f.write(f"""<property>
    <name>fs.s3.awsAccessKeyId</name>
    <value>{SECRET}</value>
    </property>

    <property>
    <name>fs.s3.awsSecretAccessKey</name>
    <value>{SECRETACCESS}</value>
    </property>
    """)
        
    import h2o
    h2o.init(jvm_custom_args=[f"-Dsys.ai.h2o.persist.s3.endPoint={URL}","-Dsys.ai.h2o.persist.s3.enable.path.style=true"],extra_classpath=["-hdfs_config core-site.xml"])

    # Set s3 credentials
    from h2o.persist import set_s3_credentials
    set_s3_credentials(SECRET, SECRETACCESS)

Note: If encouter error "botocore.errorfactory.NoSuchBucket: An error occurred (NoSuchBucket) when calling the PutObject operation: Unknown", it means the bucket `testbucket` is not found. Need to create it first.


Tested by:

- May 3, 2022: Bo Wei (bo.b.wei@seagate.com) using Cortx OVA-2.0.0-713 as S3 Server.
