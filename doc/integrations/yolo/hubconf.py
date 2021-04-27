"""YOLOv5 PyTorch Hub models https://pytorch.org/hub/ultralytics_yolov5/

Usage:
    import torch
    model = torch.hub.load('ultralytics/yolov5', 'yolov5s')
"""

from pathlib import Path

import torch

from models.yolo import Model
from utils.general import check_requirements, set_logging
from utils.google_utils import attempt_download
from utils.torch_utils import select_device

dependencies = ['torch', 'yaml']
check_requirements(Path(__file__).parent / 'requirements.txt', exclude=('pycocotools', 'thop'))


def create(name, pretrained, channels, classes, autoshape, verbose):
    """Creates a specified YOLOv5 model

    Arguments:
        name (str): name of model, i.e. 'yolov5s'
        pretrained (bool): load pretrained weights into the model
        channels (int): number of input channels
        classes (int): number of model classes

    Returns:
        pytorch model
    """
    try:
        set_logging(verbose=verbose)

        cfg = list((Path(__file__).parent / 'models').rglob(f'{name}.yaml'))[0]  # model.yaml path
        model = Model(cfg, channels, classes)
        if pretrained:
            fname = f'{name}.pt'  # checkpoint filename
            attempt_download(fname)  # download if not found locally
            ckpt = torch.load(fname, map_location=torch.device('cpu'))  # load
            msd = model.state_dict()  # model state_dict
            csd = ckpt['model'].float().state_dict()  # checkpoint state_dict as FP32
            csd = {k: v for k, v in csd.items() if msd[k].shape == v.shape}  # filter
            model.load_state_dict(csd, strict=False)  # load
            if len(ckpt['model'].names) == classes:
                model.names = ckpt['model'].names  # set class names attribute
            if autoshape:
                model = model.autoshape()  # for file/URI/PIL/cv2/np inputs and NMS
        device = select_device('0' if torch.cuda.is_available() else 'cpu')  # default to GPU if available
        return model.to(device)

    except Exception as e:
        help_url = 'https://github.com/ultralytics/yolov5/issues/36'
        s = 'Cache maybe be out of date, try force_reload=True. See %s for help.' % help_url
        raise Exception(s) from e


def custom(path_or_model='path/to/model.pt', autoshape=True, verbose=True):
    """YOLOv5-custom model https://github.com/ultralytics/yolov5

    Arguments (3 options):
        path_or_model (str): 'path/to/model.pt'
        path_or_model (dict): torch.load('path/to/model.pt')
        path_or_model (nn.Module): torch.load('path/to/model.pt')['model']

    Returns:
        pytorch model
    """
    set_logging(verbose=verbose)

    model = torch.load(path_or_model) if isinstance(path_or_model, str) else path_or_model  # load checkpoint
    if isinstance(model, dict):
        model = model['ema' if model.get('ema') else 'model']  # load model

    hub_model = Model(model.yaml).to(next(model.parameters()).device)  # create
    hub_model.load_state_dict(model.float().state_dict())  # load state_dict
    hub_model.names = model.names  # class names
    if autoshape:
        hub_model = hub_model.autoshape()  # for file/URI/PIL/cv2/np inputs and NMS
    device = select_device('0' if torch.cuda.is_available() else 'cpu')  # default to GPU if available
    return hub_model.to(device)


def yolov5s(pretrained=True, channels=3, classes=80, autoshape=True, verbose=True):
    # YOLOv5-small model https://github.com/ultralytics/yolov5
    return create('yolov5s', pretrained, channels, classes, autoshape, verbose)


def yolov5m(pretrained=True, channels=3, classes=80, autoshape=True, verbose=True):
    # YOLOv5-medium model https://github.com/ultralytics/yolov5
    return create('yolov5m', pretrained, channels, classes, autoshape, verbose)


def yolov5l(pretrained=True, channels=3, classes=80, autoshape=True, verbose=True):
    # YOLOv5-large model https://github.com/ultralytics/yolov5
    return create('yolov5l', pretrained, channels, classes, autoshape, verbose)


def yolov5x(pretrained=True, channels=3, classes=80, autoshape=True, verbose=True):
    # YOLOv5-xlarge model https://github.com/ultralytics/yolov5
    return create('yolov5x', pretrained, channels, classes, autoshape, verbose)


def yolov5s6(pretrained=True, channels=3, classes=80, autoshape=True, verbose=True):
    # YOLOv5-small-P6 model https://github.com/ultralytics/yolov5
    return create('yolov5s6', pretrained, channels, classes, autoshape, verbose)


def yolov5m6(pretrained=True, channels=3, classes=80, autoshape=True, verbose=True):
    # YOLOv5-medium-P6 model https://github.com/ultralytics/yolov5
    return create('yolov5m6', pretrained, channels, classes, autoshape, verbose)


def yolov5l6(pretrained=True, channels=3, classes=80, autoshape=True, verbose=True):
    # YOLOv5-large-P6 model https://github.com/ultralytics/yolov5
    return create('yolov5l6', pretrained, channels, classes, autoshape, verbose)


def yolov5x6(pretrained=True, channels=3, classes=80, autoshape=True, verbose=True):
    # YOLOv5-xlarge-P6 model https://github.com/ultralytics/yolov5
    return create('yolov5x6', pretrained, channels, classes, autoshape, verbose)


if __name__ == '__main__':
    model = create(name='yolov5s', pretrained=True, channels=3, classes=80, autoshape=True, verbose=True)  # pretrained
    # model = custom(path_or_model='path/to/model.pt')  # custom

    # Verify inference
    import cv2
    import numpy as np
    from PIL import Image

    imgs = ['data/images/zidane.jpg',  # filename
            'https://github.com/ultralytics/yolov5/releases/download/v1.0/zidane.jpg',  # URI
            cv2.imread('data/images/bus.jpg')[:, :, ::-1],  # OpenCV
            Image.open('data/images/bus.jpg'),  # PIL
            np.zeros((320, 640, 3))]  # numpy

    results = model(imgs)  # batched inference
    results.print()
    results.save()
