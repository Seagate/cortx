import requests
import os
from botocore.exceptions import ClientError
from elasticsearch_connector import ElasticsearchConnector


async def upload_file_to_s3(s3_client, es_client: ElasticsearchConnector, file_data, token):
    """ Gets a file from slack and uploads it to a Cortx S3 bucket

    Parameters
    ----------
    s3_client : botocore.client.S3
        A low-level client representing Cortx Simple Storage Service (S3)

    es_client : elasticsearch_connector.Elasticsearch
        Elasticsearch low-level client. Provides a straightforward mapping from Python to ES REST endpoints.

    file_data : dict
        File data from slack

    token: str
        A user access token from slack

    Returns
    ----------
    bool: Returns true if the file was uploaded to S3



    """
    # search for existing file on elastic search
    try:

        file_id = file_data['id']
        file_name = file_data['name']
        created = file_data['created']
        timestamp = file_data['timestamp']
        mimetype = file_data['mimetype']
        filetype = file_data['filetype']
        user_id = file_data['user']
        size = file_data['size']

        url = file_data['url_private']
        file_path = os.path.join(os.getcwd(), 'uploads', file_name)

        print("Saving to", file_name)

        headers = {'Authorization': 'Bearer ' + token}

        r = requests.get(url, headers=headers)

        with open(file_path, 'wb') as f:
            for chunk in r.iter_content(chunk_size=1024):
                if chunk:
                    f.write(chunk)
        if os.path.exists(file_path):
            response = s3_client.upload_file(
                file_path, 'testbucket', file_name)
            es_client.create_doc(file_id=file_id, file_name=file_name, created=created,
                                 timestamp=timestamp, mimetype=mimetype, filetype=filetype, user_id=user_id, size=size)

        if os.path.exists(file_path):
            os.remove(path=file_path)
        print("File uploaded to S3 with key {}".format(file_name))
        # print(response)

    except ClientError as e:
        print("Couldn't upload to s3")
        print(e)
        return False
    except Exception as e:
        print(e)
        return False
    return True



