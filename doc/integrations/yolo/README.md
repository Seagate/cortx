
## Description

This repository represents open-source detection methods with YOLOv5 at the edge (on camera), sending cloud storage and importing data.
YOLO model trains on the COCO dataset and can detect up to 80 classes.
With the help of this model, I detect objects on video frames and save the bounding boxes in a text file, send them to the CORTX cloud, and store them there.\
Next, I download the data from the CORTX storage and draw the bounding boxes on the original video. \
NOTE: In Yolo, the coordinates are relative. Meaning that the annotations are written this way: \
<object-class, x_center, y_center, width, height>

## Concept pitch
can be found [here](https://www.loom.com/share/4c0956c5851249db8119a0fdaa7f2d16).


The left top gif is the original film.\
The right top gif is the yolo detection output.\
The center down gif is the output after import the data from the cloud.\
The last image show all the text files that i upload into CORTX cloud using S3 client Cyberduck.

<p align="center">
   <img src="./gif/original.gif">
   <img src="./gif/yoloResults.gif">
   <img src="./gif/image.png">
   <img src="./gif/outputAfterReceive.gif">
</p>

## Requirements

Python 3.8 or later with all [requirements.txt](https://github.com/ultralytics/yolov5/blob/master/requirements.txt) dependencies installed, including `torch>=1.7`. To install run:
```bash
$ pip install -r requirements.txt
$ pip instll boto3
$ pip instll logging
$ pip instll botocore
```

## Inference

`detectAndSend.py` runs inference, downloading models automatically from the [latest YOLOv5 release](https://github.com/ultralytics/yolov5/releases) and saving results to `runs/detect`.

```bash
$ python detectAndSend.py --source 0  # webcam
                            file.jpg  # image 
                            file.mp4  # video
                            path/  # directory
                            path/*.jpg  # glob
                            'https://youtu.be/NUsoVlDFqZg'  # YouTube video
                            'rtsp://example.com/media.mp4'  # RTSP, RTMP, HTTP stream
```

```bash
$ python detectAndSend.py  --source test.mp4 --weights yolov5s.pt --conf 0.25 --save-txt
```

## Future work
Add RTSP(Real Time Streaming Protocol)
Improve the model

