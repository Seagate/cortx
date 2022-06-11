from flask import Flask, render_template
from flask_bootstrap import Bootstrap
import boto3
from filters import datetimeformat

app = Flask(__name__)
Bootstrap(app)
app.jinja_env.filters['datetimeformat'] = datetimeformat



@app.route('/')
def files():
    s3_resource = boto3.resource(
        "s3",
        endpoint_url='', 
        aws_access_key_id='',
        aws_secret_access_key=''
    )

    my_bucket = s3_resource.Bucket('covidxraybucket')
    print('my_bucket!', my_bucket)



    summaries = my_bucket.objects.all()
    print('summaries!', summaries)

    for f in summaries:
        print(f.key)

    return render_template('index.html', my_bucket=my_bucket, files=summaries)


if __name__ == "__main__":
    app.run()
