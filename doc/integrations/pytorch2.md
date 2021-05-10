# PyTorch meets CORTX
[![made-with-python](https://img.shields.io/badge/Made%20with-Python-1f425f.svg)](https://www.python.org/)
## Facebook Research ParlAI conversational AI training and testing pipeline using CORTX S3 with Flask app integration
### Demo Link
https://www.youtube.com/watch?v=YUS7YCJ9rgs

[![IMAGE ALT TEXT](http://img.youtube.com/vi/YUS7YCJ9rgs/0.jpg)](http://www.youtube.com/watch?v=YUS7YCJ9rgs "CORTX Meets PyTorch ParlAI")

### Model saving for continous fine tuning
![alt text](https://github.com/kishorkuttan/cortx/blob/main/doc/integrations/pytorch/diagram.png)

**What is ParlAI?**

ParlAI is a python-based platform for enabling dialog AI research.

Its goal is to provide researchers:

  * a unified framework for sharing, training and testing dialog models

  *  many popular datasets available all in one place, with the ability to multi-task over them

  * seamless integration of Amazon Mechanical Turk for data collection and human evaluation

  * integration with chat services like Facebook Messenger to connect agents with humans in a chat interface
link: https://github.com/facebookresearch/ParlAI

In ParlAI, we call an environment a world. In each world, there are agents. Examples of agents include models and datasets. Agents interact with each other by taking turns acting and observing acts.

To concretize this, we’ll consider the train loop used to train a Image Seq2Seq model trained on all DodecaDialogue tasks and fine-tuned on the Empathetic Dialogue task, We call this train environment a world, and it contains two agents - the seq2seq model and the dataset. The model and dataset agents interact with each other in this way: the dataset acts first and outputs a batch of train examples with the correct labels. The model observes this act to get the train examples, and then acts by doing a single train step on this batch (predicting labels and updating its parameters according to the loss). The dataset observes this act and outputs the next batch, and so on.

**CORTX S3 support for ParlAI fine tuning**

1. During training the latest models get uploaded to the CORTX S3. Which can be downloaded from the bucket for Flask RESTful API integration or web app depolyment.
2. This strategy helps in different versions of trained model for custom datasets withoud worrying the larger model size(>4GB)

# RUN

I just used private virtual lab through CloudShare with **Windows Server 2019 Standard** as the system.(https://github.com/Seagate/cortx/wiki/CORTX-Cloudshare-Setup-for-April-Hackathon-2021). You can also Use standard QUICK_START guide for installation.(https://github.com/Seagate/cortx/blob/main/QUICK_START.md)

**Installation setup**

1. Install anaconda python. (https://anaconda.org/)
2. create a conda environment
``` 
conda create -n cortx pip python=3.7
```
3. activate the environment
``` 
conda activate cortx
```
4. Install the requirements
``` 
pip install -r requirements.txt
```

**Training**
1. Create an S3 bucket to store the weight files.
``` 
import boto3
from botocore.client import Config
END_POINT_URL = "http://192.168.x.xxx"
A_KEY = "AKIAtEpiGWUcQIelPRlD1Pi6xQ"

S_KEY = "YNV6xS8lXnCTGSy1x2vGkmGnmdJbZSapNXaSaRhK"


s3_client = boto3.client('s3', endpoint_url=END_POINT_URL,
                         aws_access_key_id=A_KEY,
                         aws_secret_access_key=S_KEY,
                         config=Config(signature_version='s3v4'),
                         region_name='US')
def create_bucket_op(bucket_name, region):
    if region is None:
        s3_client.create_bucket(Bucket=bucket_name)
    else:
        location = {'LocationConstraint': region}
        s3_client.create_bucket(Bucket=bucket_name,
                                CreateBucketConfiguration=location)
create_bucket_op("testing","US")
print("bucket created")
```
2. Train

``` 
cd training
python train.py
```
The "training" directory include a custom dataset "train.txt" in ParlAI format. Where the "text:" the user message or message from Agent 1, "label:" is the actual message from the AI agent or trained model or Agent 2. The model after geting fine tuned by reducing the loss, can able to predict the label. "episode_done:True" will stops the conversation thread to start a new thread. After training the model gets uploaded to S3 bucket.

train.txt

``` 

text:what is CORTX?   labels:CORTX is a distributed object storage system designed for great efficiency, massive capacity, and high HDD-utilization
text:is it open sourced?   labels:CORTX is 100% Open Source
text:Does it works with any processor   labels:Yes, it works with any processor.
text:is it flexible?   labels:Highly flexible, works with HDD, SSD, and NVM.
text:is it scalable?   labels:Massively Scalable. Scales up to a billion billion billion billion billion exabytes (2^206) and 1.3 billion billion billion billion (2^120) objects with unlimited object sizes.
text:is it responsive?   labels:Rapidly Responsive. Quickly retrieves data regardless of the scale using a novel Key-Value System that ensures low search latency across massive data sets.   
text:how much resiliant?   labels:Highly flexible, works with HDD, SSD, and NVM.
text:bye   labels:bye.   episode_done=True
```
![alt text](https://github.com/kishorkuttan/cortx/blob/main/doc/integrations/pytorch/t2.png)

![alt text](https://github.com/kishorkuttan/cortx/blob/main/doc/integrations/pytorch/t4.png)
**Flask web application**



![alt text](https://github.com/kishorkuttan/cortx/blob/main/doc/integrations/pytorch/flask.png)
![alt text](https://github.com/kishorkuttan/cortx/blob/main/doc/integrations/pytorch/f1.png)
``` 
cd ..
python application.py  -mf "training/poly-encoder/model" 
```
1. For each user there will be a unique id to seperate conversations. 
2. Each connection will include one world, one AI agent and multiple users.
3. The fine tuned model will be downloaded from the CORTX S3 bucket for inference in Flask.



## Reference
1. https://github.com/facebookresearch/ParlAI
2. https://parl.ai/docs/zoo.html
3. https://arxiv.org/abs/1905.01969
4. https://parl.ai/projects/polyencoder/ 




