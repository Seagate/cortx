import os
from PIL import Image


def resize(file_path="./.tmp/bg.jpg", size=(128, 128)):
    try:
        file_name = os.path.basename(file_path).split(".")[0]
        outfile = "./.tmp/{}_{}x{}.jpg".format(file_name, size[0], size[1])
        im = Image.open(file_path)
        im.thumbnail(size, Image.ANTIALIAS)
        im.save(outfile, "JPEG")

        return outfile
    except Exception:
        return None
