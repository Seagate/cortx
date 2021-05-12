# CORTX Pytorch Integration

Video Link: https://youtu.be/cS-f7MNpXyo

## Inspiration

As a fan of machine learning and deep learning, I like the fact that the CORTX project aims to solve the challenge of storing and accessing large amounts of data. This is a problem that is common in the Machine Learning and Deep Learning space.

## What it does
My project is a Pytorch Integration for CORTX. It demonstrates a workflow that uses CORTX as a backing store for storing a dataset. We also built a Dataloader that uses CORTX buckets to store per class data. The integration eases the machine learning workflow so that you can train a deep learning model by just supplying the access information for your CORTX instance as well as the list of bucket names that contain your per-class data

## Challenges we ran into

A couple of challenges with the CORTX instance on CloudShare but all were fixed by communicating on the Slack channel.
Built With

    cortx
    jupyter
    python
    pytorch

Try it out

    GitHub Repo


## Getting Started

To use this integration, you just need to clone [the notebook](cortx-med-mnist.ipynb) and run the cells. A few dependencies you'll need in your python environment:

+ Pytorch 1.7
+ Boto3
+ Matplotlib

You can just install these in your environment using a pip install.

This notebook has been developed using Python 3.8
