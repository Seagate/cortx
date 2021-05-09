import os
from botocore.exceptions import ClientError
from elasticsearch_connector import ElasticsearchConnector


async def get_file_from_s3(s3_client, es_client: ElasticsearchConnector, file_name):
    """ Gets a file from a Cortx S3 bucket and uploads it to slack

    Parameters
    ----------
    s3_client : botocore.client.S3
        A low-level client representing Cortx Simple Storage Service (S3)

    es_client : elasticsearch_connector.Elasticsearch
        Elasticsearch low-level client. Provides a straightforward mapping from Python to ES REST endpoints.

    file_name: str
        File name

    Returns
    ----------
    bool: Returns true if the file was downloaded from S3

    """
    try:
        if(es_client.check_if_doc_exists(file_name=file_name)):
            # if(True):
            file_path = os.path.join(os.getcwd(), 'downloads', file_name)
            response = s3_client.download_file(
                Bucket='testbucket', Key=file_name, Filename=file_path)
        else:
            print("File not found in ES")
            return False
    except ClientError as e:
        print("Couldn't get file from s3")
        return False
    except Exception as e:
        print(e)
        return False
    return True
