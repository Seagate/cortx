FROM ubuntu:20.04

RUN apt update
RUN apt upgrade

# Set timezone:
RUN ln -snf /usr/share/zoneinfo/$CONTAINER_TIMEZONE /etc/localtime && echo $CONTAINER_TIMEZONE > /etc/timezone

# Install dependencies:
RUN apt-get install -y tzdata s3fs wget
RUN apt-get install -y awscli 

COPY creds ${HOME}/.passwd-s3fs
RUN chmod 600 ${HOME}/.passwd-s3fs

RUN mkdir cortx-fs 
COPY aws_creds ~/.aws/credentials
COPY aws_config ~/.aws/aws_config