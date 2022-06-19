from __future__ import print_function
import cv2 as cv
import numpy as np
import argparse
import random as rng
rng.seed(12345)
import os
import sys
import threading

import boto3
import logging
import shutil
from botocore.client import Config
from matplotlib import pyplot as plt
from botocore.exceptions import ClientError
from boto3.s3.transfer import TransferConfig

END_POINT_URL = 'http://uvo1baooraa1xb575uc.vm.cld.sr/'
A_KEY = 'AKIAtEpiGWUcQIelPRlD1Pi6xQ'
S_KEY = 'YNV6xS8lXnCTGSy1x2vGkmGnmdJbZSapNXaSaRhK'

# print(cv.getBuildInformation())

data_bbox = {
    "label": "person",
    "bbox": [10, 20, 100, 200], # x,y,w,h
}

def upload_download_op_file(bucket_name, file_name, file_location,
                            region, operation):
    if not file_location:
        logging.error('The file location %d is missing for %s operation!'
                      % (file_location, operation))
        return False
    if operation == 'download':
        s3_resource.Bucket(bucket_name).download_file(file_name, file_location)
    elif operation == 'upload' and region is None:
        s3_resource.Bucket(bucket_name).upload_file(file_location, file_name)
    else:
        location = {'LocationConstraint': region}
        s3_resource.Bucket(bucket_name
                           ).upload_file(file_location, file_name,
                                         CreateBucketConfiguration=location)
    return True

def delete_op_file(bucket_name, file_name, operation):
    if not file_name:
        logging.error('The file name %s is missing for%s operation!'
                      % (file_name, operation))
        return False
    s3_client.delete_object(Bucket=bucket_name, Key=file_name)
    return True


def file_operation(bucket_name=None, file_name=None, file_location=None,
                   region=None, operation='list'):
    if not bucket_name:
        logging.error('The bucket name is %s missing!' % (bucket_name))
        return False
    try:
        if operation == 'list':
            return list_op_file(bucket_name)
        elif operation == 'delete':
            return delete_op_file(bucket_name, file_name, operation)
        elif operation == 'upload' or operation == 'download':
            return upload_download_op_file(bucket_name, file_name,
                                           file_location, region, operation)
        else:
            logging.error('unknown file operation')
            return False
    except ClientError as e:
        logging.error(e)
        return False
    return True

def file_operation(bucket_name=None, file_name=None, file_location=None,
                   region=None, operation='list'):
    if not bucket_name:
        logging.error('The bucket name is %s missing!' % (bucket_name))
        return False
    try:
        if operation == 'list':
            return list_op_file(bucket_name)
        elif operation == 'delete':
            return delete_op_file(bucket_name, file_name, operation)
        elif operation == 'upload' or operation == 'download':
            return upload_download_op_file(bucket_name, file_name,
                                           file_location, region, operation)
        else:
            logging.error('unknown file operation')
            return False
    except ClientError as e:
        logging.error(e)
        return False
    return True

def list_op_file(bucket_name):
    current_bucket = s3_resource.Bucket(bucket_name)
    print('The files in bucket %s:\n' % (bucket_name))
    for obj in current_bucket.objects.all():
        print(obj.meta.data)

    return True

labels = {
    0: "person"
}

def draw_bbox(image, line):
    # print(image.shape)
    height, width, *rest = image.shape
    label, *bbox = line.split(" ")
    x_center, y_center, w, h = (float(j) for j in bbox)
    w = w * width
    h = h * height
    x = x_center * width - w / 2
    y = y_center * height - h / 2
    label_text = labels.get(int(label))
    color = (0, 0, 255)
    # print(label_text, x, y, w, h)
    cv.rectangle(image, (int(x), int(y), int(x + w), int(y + h)), color)
    if label_text:
        cv.putText(image, label_text, (int(x), int(y + 10)), cv.FONT_HERSHEY_SIMPLEX, 0.5, color, 1, cv.LINE_AA)
    # bbox = data.get("bbox")
    # if bbox:
    #     # cv.rectangle(image, (int(bbox[0]), int(bbox[1]), int(bbox[0] + bbox[2]), int(bbox[1] + bbox[3])), color)
    #     cv.rectangle(image, (int(bbox[0]), int(bbox[1]), int(bbox[2]), int(bbox[3])), color)
    #     label = data.get("label")
    #     if label:
    #         cv.putText(image, label, (int(bbox[0]), int(bbox[1] + 10)), cv.FONT_HERSHEY_SIMPLEX, 0.5, color, 1, cv.LINE_AA)
    # return image

s3_resource = boto3.resource('s3', endpoint_url=END_POINT_URL,
                             aws_access_key_id=A_KEY,
                             aws_secret_access_key=S_KEY,
                             config=Config(signature_version='s3v4'),
                             region_name='US')

s3_client = boto3.client('s3', endpoint_url=END_POINT_URL,
                         aws_access_key_id=A_KEY,
                         aws_secret_access_key=S_KEY,
                         config=Config(signature_version='s3v4'),
                         region_name='US')

bucket_name = 'detection'
file_name = r'0_1.txt'
# path_file_upload = r'C:\PycharmProjects\cortxHackton\upload\0_5.txt'
# assert os.path.isfile(path_file_upload)
# with open(path_file_upload, "r") as f:
#     pass
path_file_download = r'download\0_1.txt'
path_save = ''

if file_operation(bucket_name, file_name, path_file_download, None, 'download'):
    print("Downloading the file to S3 has been completed successfully!")


cap = cv.VideoCapture("test.mp4")
output = None
video_name = "test.mp4"
i = 1
while True:
    has_detection = False
    ret, frame = cap.read()
    file_save = rf"download\new_out.mp4_{i}.txt"
    if file_operation(bucket_name, f"{video_name}_{i}.txt", file_save, None, 'download'):
        i += 1
        has_detection = True

    if not output:
        output = cv.VideoWriter("out.mp4", cv.VideoWriter_fourcc(*'MP4V'), 10, (frame.shape[1], frame.shape[0]))

    if not ret:
        print("done")
        break
    # Our operations on the frame come here

    # gray = cv.cvtColor(frame, cv.COLOR_BGR2GRAY)
    # draw_bbox(gray, data_bbox)
    if has_detection:
        with open(file_save) as f:
            for l in f.readlines():
                draw_bbox(frame, l)

    if output:
        output.write(frame)
    cv.imshow('frame', frame)
    if cv.waitKey(1) & 0xFF == ord('q'):
        break