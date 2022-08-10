import argparse
import time
from pathlib import Path
import cv2
import torch
import torch.backends.cudnn as cudnn
from numpy import random
from models.experimental import attempt_load
from utils.datasets import LoadStreams, LoadImages
from utils.general import check_img_size, check_requirements, check_imshow, non_max_suppression, apply_classifier, \
    scale_coords, xyxy2xywh, strip_optimizer, set_logging, increment_path, save_one_box
from utils.plots import plot_one_box
from utils.torch_utils import select_device, load_classifier, time_synchronized
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

END_POINT_URL = 'http://uvo1baooraa1xb575uc.vm.cld.sr'
A_KEY = 'AKIAtEpiGWUcQIelPRlD1Pi6xQ'
S_KEY = 'YNV6xS8lXnCTGSy1x2vGkmGnmdJbZSapNXaSaRhK'

class ProgressPercentage(object):
    def __init__(self, filename):
        self._filename = filename
        self._size = float(os.path.getsize(filename))
        self._seen_so_far = 0
        self._lock = threading.Lock()

    def __call__(self, bytes_amount):
        # To simplify, assume this is hooked up to a single filename
        with self._lock:
            self._seen_so_far += bytes_amount
            percentage = (self._seen_so_far / self._size) * 100
            sys.stdout.write("\r%s  %s / %s  (%.2f%%)" %
                             (self._filename, self._seen_so_far,
                              self._size, percentage))
            sys.stdout.flush()

"""Functions for buckets operation"""
def create_bucket_op(bucket_name, region):
    if region is None:
        s3_client.create_bucket(Bucket=bucket_name)
    else:
        location = {'LocationConstraint': region}
        s3_client.create_bucket(Bucket=bucket_name,
                                CreateBucketConfiguration=location)


def list_bucket_op(bucket_name, region, operation):
    buckets = s3_client.list_buckets()
    if buckets['Buckets']:
        for bucket in buckets['Buckets']:
            print(bucket)
            return True
    else:
        logging.error('unknown bucket operation')
        return False


def bucket_operation(bucket_name, region=None, operation='list'):
    try:
        if operation == 'delete':
            s3_client.delete_bucket(Bucket=bucket_name)
        elif operation == 'create':
            create_bucket_op(bucket_name, region)
        elif operation == 'list':
            return list_bucket_op(bucket_name, region, operation)
        else:
            logging.error('unknown bucket operation')
            return False
    except ClientError as e:
        logging.error(e)
        return False
    return True


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


"""Functions for files operation"""


def list_op_file(bucket_name):
    current_bucket = s3_resource.Bucket(bucket_name)
    print('The files in bucket %s:\n' % (bucket_name))
    for obj in current_bucket.objects.all():
        print(obj.meta.data)

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


def detect(opt):
    source, weights, view_img, save_txt, imgsz = opt.source, opt.weights, opt.view_img, opt.save_txt, opt.img_size
    save_img = not opt.nosave and not source.endswith('.txt')  # save inference images
    webcam = source.isnumeric() or source.endswith('.txt') or source.lower().startswith(
        ('rtsp://', 'rtmp://', 'http://', 'https://'))

    # Directories
    save_dir = increment_path(Path(opt.project) / opt.name, exist_ok=opt.exist_ok)  # increment run
    (save_dir / 'labels' if save_txt else save_dir).mkdir(parents=True, exist_ok=True)  # make dir

    # Initialize
    set_logging()
    device = select_device(opt.device)
    half = device.type != 'cpu'  # half precision only supported on CUDA

    # Load model
    model = attempt_load(weights, map_location=device)  # load FP32 model
    stride = int(model.stride.max())  # model stride
    imgsz = check_img_size(imgsz, s=stride)  # check img_size
    if half:
        model.half()  # to FP16

    # Second-stage classifier
    classify = False
    if classify:
        modelc = load_classifier(name='resnet101', n=2)  # initialize
        modelc.load_state_dict(torch.load('weights/resnet101.pt', map_location=device)['model']).to(device).eval()

    # Set Dataloader
    vid_path, vid_writer = None, None
    if webcam:
        view_img = check_imshow()
        cudnn.benchmark = True  # set True to speed up constant image size inference
        dataset = LoadStreams(source, img_size=imgsz, stride=stride)
    else:
        dataset = LoadImages(source, img_size=imgsz, stride=stride)

    # Get names and colors
    names = model.module.names if hasattr(model, 'module') else model.names
    colors = [[random.randint(0, 255) for _ in range(3)] for _ in names]

    # Run inference
    if device.type != 'cpu':
        model(torch.zeros(1, 3, imgsz, imgsz).to(device).type_as(next(model.parameters())))  # run once
    t0 = time.time()
    for path, img, im0s, vid_cap in dataset:
        img = torch.from_numpy(img).to(device)
        img = img.half() if half else img.float()  # uint8 to fp16/32
        img /= 255.0  # 0 - 255 to 0.0 - 1.0
        if img.ndimension() == 3:
            img = img.unsqueeze(0)

        # Inference
        t1 = time_synchronized()
        pred = model(img, augment=opt.augment)[0]

        # Apply NMS
        pred = non_max_suppression(pred, opt.conf_thres, opt.iou_thres, classes=opt.classes, agnostic=opt.agnostic_nms)
        t2 = time_synchronized()

        # Apply Classifier
        if classify:
            pred = apply_classifier(pred, modelc, img, im0s)

        # Process detections
        for i, det in enumerate(pred):  # detections per image
            if webcam:  # batch_size >= 1
                p, s, im0, frame = path[i], '%g: ' % i, im0s[i].copy(), dataset.count
            else:
                p, s, im0, frame = path, '', im0s.copy(), getattr(dataset, 'frame', 0)

            p = Path(p)  # to Path
            save_path = str(save_dir / p.name)  # img.jpg
            txt_path = str(save_dir / 'labels' / p.stem) + ('' if dataset.mode == 'image' else f'_{frame}')  # img.txt
            s += '%gx%g ' % img.shape[2:]  # print string
            gn = torch.tensor(im0.shape)[[1, 0, 1, 0]]  # normalization gain whwh
            if len(det):
                # Rescale boxes from img_size to im0 size
                det[:, :4] = scale_coords(img.shape[2:], det[:, :4], im0.shape).round()

                # Print results
                for c in det[:, -1].unique():
                    n = (det[:, -1] == c).sum()  # detections per class
                    s += f"{n} {names[int(c)]}{'s' * (n > 1)}, "  # add to string

                # Write results
                for *xyxy, conf, cls in reversed(det):
                    if save_txt:  # Write to file
                        xywh = (xyxy2xywh(torch.tensor(xyxy).view(1, 4)) / gn).view(-1).tolist()  # normalized xywh
                        line = (cls, *xywh, conf) if opt.save_conf else (cls, *xywh)  # label format
                        file_name = str(source) + '_' + str(frame) + '.txt'
                        with open(txt_path + '.txt', 'a') as f:
                            f.write(('%g ' * len(line)).rstrip() % line + '\n')
                        if file_operation(bucket_name, file_name, txt_path + ".txt", None, 'upload'):
                            print("Uploading file to S3 completed successfully!")

                    if save_img or opt.save_crop or view_img:  # Add bbox to image
                        c = int(cls)  # integer class
                        label = None if opt.hide_labels else (names[c] if opt.hide_conf else f'{names[c]} {conf:.2f}')

                        plot_one_box(xyxy, im0, label=label, color=colors[c], line_thickness=opt.line_thickness)
                        if opt.save_crop:
                            save_one_box(xyxy, im0s, file=save_dir / 'crops' / names[c] / f'{p.stem}.jpg', BGR=True)

            # Print time (inference + NMS)
            print(f'{s}Done. ({t2 - t1:.3f}s)')

            # Stream results
            if view_img:
                cv2.imshow(str(p), im0)
                file_name = str(source) + '_' + str(frame) + '.txt'
                # path_file_upload = r"C:\PycharmProjects\yolov5\runs\detect\{}\labels\{}\{}_{}".format(save_dir, s, source, frame)
                if file_operation(bucket_name, file_name, txt_path + ".txt", None, 'upload'):
                    print("Uploading file to S3 completed successfully!")
                cv2.waitKey(1)  # 1 millisecond

            # Save results (image with detections)
            if save_img:
                if dataset.mode == 'image':
                    cv2.imwrite(save_path, im0)
                else:  # 'video' or 'stream'
                    if vid_path != save_path:  # new video
                        vid_path = save_path
                        if isinstance(vid_writer, cv2.VideoWriter):
                            vid_writer.release()  # release previous video writer
                        if vid_cap:  # video
                            fps = vid_cap.get(cv2.CAP_PROP_FPS)
                            w = int(vid_cap.get(cv2.CAP_PROP_FRAME_WIDTH))
                            h = int(vid_cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
                        else:  # stream
                            fps, w, h = 30, im0.shape[1], im0.shape[0]
                        save_path += '.mp4'
                        vid_writer = cv2.VideoWriter(save_path, cv2.VideoWriter_fourcc(*'mp4v'), fps, (w, h))
                    vid_writer.write(im0)

    if save_txt or save_img:
        s = f"\n{len(list(save_dir.glob('labels/*.txt')))} labels saved to {save_dir / 'labels'}" if save_txt else ''
        print(f"Results saved to {save_dir}{s}")

    print(f'Done. ({time.time() - t0:.3f}s)')


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--weights', nargs='+', type=str, default='yolov5s.pt', help='model.pt path(s)')
    parser.add_argument('--source', type=str, default='data/images', help='source')  # file/folder, 0 for webcam
    parser.add_argument('--img-size', type=int, default=640, help='inference size (pixels)')
    parser.add_argument('--conf-thres', type=float, default=0.25, help='object confidence threshold')
    parser.add_argument('--iou-thres', type=float, default=0.45, help='IOU threshold for NMS')
    parser.add_argument('--device', default='', help='cuda device, i.e. 0 or 0,1,2,3 or cpu')
    parser.add_argument('--view-img', action='store_true', help='display results')
    parser.add_argument('--save-txt', action='store_true', help='save results to *.txt')
    parser.add_argument('--save-conf', action='store_true', help='save confidences in --save-txt labels')
    parser.add_argument('--save-crop', action='store_true', help='save cropped prediction boxes')
    parser.add_argument('--nosave', action='store_true', help='do not save images/videos')
    parser.add_argument('--classes', nargs='+', type=int, help='filter by class: --class 0, or --class 0 2 3')
    parser.add_argument('--agnostic-nms', action='store_true', help='class-agnostic NMS')
    parser.add_argument('--augment', action='store_true', help='augmented inference')
    parser.add_argument('--update', action='store_true', help='update all models')
    parser.add_argument('--project', default='runs/detect', help='save results to project/name')
    parser.add_argument('--name', default='exp', help='save results to project/name')
    parser.add_argument('--exist-ok', action='store_true', help='existing project/name ok, do not increment')
    parser.add_argument('--line-thickness', default=3, type=int, help='bounding box thickness (pixels)')
    parser.add_argument('--hide-labels', default=False, action='store_true', help='hide labels')
    parser.add_argument('--hide-conf', default=False, action='store_true', help='hide confidences')
    opt = parser.parse_args()
    print(opt)

    check_requirements(exclude=('pycocotools', 'thop'))

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

    # assert os.path.isfile(path_file_upload)
    # with open(path_file_upload, "r") as f:
    #     pass
    path_file_download = r'download\test.txt'
    path_save = ''

    if bucket_operation(bucket_name, None, 'create'):
        print("Bucket creation completed successfully!")

    with torch.no_grad():
        if opt.update:  # update all models (to fix SourceChangeWarning)
            for opt.weights in ['yolov5s.pt', 'yolov5m.pt', 'yolov5l.pt', 'yolov5x.pt']:
                detect(opt=opt)
                strip_optimizer(opt.weights)
        else:
            detect(opt=opt)
