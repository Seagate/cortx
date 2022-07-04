import boto3
import ipfshttpclient
#client = ipfshttpclient.connect('/ip4/127.0.0.1/tcp/5001')  # Connects to: /dns/localhost/tcp/5001/http
#res = client.add('data.txt')
#print(res)

ACCESS_KEY = 'sgiamadmin'
SECRET_ACCESS_KEY = 'ldapadmin'
END_POINT_URL = 'http://192.168.1.16:31949'
BUCKET = "pictures"

def upload_file(file_name, bucket):
    object_name = file_name
    
    s3_client = boto3.client('s3',
            endpoint_url=END_POINT_URL,
            aws_access_key_id=ACCESS_KEY,
            aws_secret_access_key=SECRET_ACCESS_KEY
    )
    
    response = s3_client.upload_file(file_name, bucket, object_name)
    #client = ipfshttpclient.connect('/ip4/127.0.0.1/tcp/5001')
    #res = client.add(file_name)
    
    return response

def list_files(bucket):
    s3_client = boto3.client('s3',
        endpoint_url=END_POINT_URL,
        aws_access_key_id=ACCESS_KEY,
        aws_secret_access_key=SECRET_ACCESS_KEY
    )
    contents = []
    try:
        for item in s3_client.list_objects(Bucket=bucket)['Contents']:
            contents.append(item)
    except Exception as e:
        pass
    return contents

def show_image(bucket):
    s3_client = boto3.client('s3',
        endpoint_url=END_POINT_URL,
        aws_access_key_id=ACCESS_KEY,
        aws_secret_access_key=SECRET_ACCESS_KEY
    )
    public_urls = []
    try:
        for item in s3_client.list_objects(Bucket=bucket)['Contents']:
            presigned_url = s3_client.generate_presigned_url('get_object', Params = {'Bucket': bucket, 'Key': item['Key']}, ExpiresIn = 100)
            public_urls.append(presigned_url)
    except Exception as e:
        pass
    return public_urls
