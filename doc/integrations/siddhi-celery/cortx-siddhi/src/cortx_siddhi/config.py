import boto3

S3_A_KEY = 'nADhGwhbSwKOCmEwvdnncA'
S3_S_KEY = 'klBIOoTa6tfzeo2q3f4nWmvBYhlvKTlvAJs7EKpz'
S3_URL = 'http://uvo10ut03fd4wh1zwlk.vm.cld.sr/'

LOGS_BUCKET = 'logs'

def get_client():
    return boto3.client('s3', endpoint_url=S3_URL,
                             aws_access_key_id=S3_A_KEY,
                             aws_secret_access_key=S3_S_KEY)


