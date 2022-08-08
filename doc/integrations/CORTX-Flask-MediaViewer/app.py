from flask import Flask, render_template, request, redirect
from werkzeug.utils import secure_filename
import os
from s3_functions import upload_file, show_image
import sys

app = Flask(__name__)

ACCESS_KEY = ''
SECRET_ACCESS_KEY = ''
END_POINT_URL = 'http://192.168.1.16:31949' 
UPLOAD_FOLDER = "uploads"
BUCKET = "pictures"

@app.route("/")
def home():
    #contents = list_files(BUCKET)
    contents = show_image(BUCKET)

    return render_template('index.html', contents=contents)

@app.route("/pics")
def list_images():
    contents = show_image(BUCKET)
    return render_template('collection.html', contents=contents)

@app.route("/upload", methods=['POST'])
def upload():
    if request.method == "POST":
        print(request.files, file=sys.stderr)
        f = request.files['file']
        f.save(os.path.join(UPLOAD_FOLDER, secure_filename(f.filename)))
        upload_file(f"uploads/{f.filename}", BUCKET)
        return redirect("/")

if __name__ == '__main__':
    app.run(debug=True)
