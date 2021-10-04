import os
import settings
import flask
from flask import send_file, request, abort, render_template
from functools import wraps
import bucket
import image

app = flask.Flask(__name__)
app.config["DEBUG"] = True


def require_api_key(view_function):
    @wraps(view_function)
    def decorated_function(*args, **kwargs):
        api_key = os.environ.get('api_key')
        if request.headers.get('x-api-key') and request.headers.get('x-api-key') == api_key:
            return view_function(*args, **kwargs)
        elif request.args.get('key') and request.args.get('key') == api_key:
            return view_function(*args, **kwargs)
        else:
            abort(401)
    return decorated_function


@app.route('/images/<size>/<filename>', methods=['GET'])
def get_resized_image(size, filename):
    w, h = size.split("x")
    _, ext = filename.split(".")
    filename_with_size = "{}_{}x{}.jpg".format(filename, w, h)
    file_path = bucket.download(filename_with_size)
    if file_path:
        return send_file(file_path, mimetype='image/{}'.format(ext))
    file_path = bucket.download(filename)
    if not file_path:
        return abort(404)
    resized_file_path = image.resize(file_path, (int(w), int(h)))
    bucket.upload(resized_file_path, filename)

    return send_file(resized_file_path, mimetype='image/{}'.format(ext))


@app.route('/images/<filename>', methods=['GET'])
def get_image(filename):
    _, ext = filename.split(".")
    file_path = bucket.download(filename)
    if not file_path:
        return abort(404)

    return send_file(file_path, mimetype='image/{}'.format(ext))


@app.route('/images', methods=['post'])
@require_api_key
def upload():
    if request.files['image'].filename != '':
        image = request.files['image']
        file_path = './.tmp/{}'.format(image.filename)
        image.save(file_path)
        uploaded = bucket.upload(file_path, image.filename)
        if uploaded:
            return "uploaded"
        return abort(400)
    return abort(500)


@app.route('/')
def home():
    return render_template('index.html')


@app.route('/api/docs')
def docs():
    return render_template('docs.html')


app.run()
