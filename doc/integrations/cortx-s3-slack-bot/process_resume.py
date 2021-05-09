import os
import boto3
import textract
from pathlib import Path
from csv import writer
from dotenv import load_dotenv

env_path = Path('.')/'.env'
load_dotenv(dotenv_path=env_path)


def process_resume(textract_client, comprehend_client, file_name, s3_client=None, es_client=None):
    """Get the file from S3, extract the text from .jpeg or .pdf file using textract and we detect PII using AWS comprehend to get the information like Name, Email, Phone, Address from the file and add it to resume-data.csv


    Parameters
    ----------
    textract_client : Textract.Client
        Amazon Textract detects and analyzes text in documents and converts it into machine-readable text. This is the API reference documentation for Amazon Textract.

    comprehend_client : Comprehend.Client
        Amazon Comprehend is an AWS service for gaining insight into the content of documents. Use these actions to determine the topics contained in your documents, the topics they discuss, the predominant sentiment expressed in them, the predominant language used, and more.

    file_name: str
        A filename

    s3_client : botocore.client.S3
        A low-level client representing Cortx Simple Storage Service (S3)

    es_client : elasticsearch_connector.Elasticsearch
        Elasticsearch low-level client. Provides a straightforward mapping from Python to ES REST endpoints.


    """
    file_path = os.path.join(os.getcwd(), 'downloads', file_name)
    file_name, file_extension = os.path.splitext(file_path)

    text = ""

    if file_extension == '.pdf':
        file_bytes = textract.process(file_path, method='pdfminer')
        text = file_bytes.decode("utf-8")
    else:
        with open(file_path, 'rb') as document:
            imageBytes = bytearray(document.read())
        response = textract_client.detect_document_text(
            Document={'Bytes': imageBytes})
        # Print text
        print("\nText\n========")
        for item in response["Blocks"]:
            if item["BlockType"] == "LINE":
                # print('\033[94m' + item["Text"] + '\033[0m')
                text = text + " " + item["Text"] + "\n"

    entities = comprehend_client.detect_pii_entities(
        LanguageCode="en", Text=text)

    print(entities)
    person_data: dict = {
    }

    # print("\nEntities\n========")
    for entity in entities["Entities"]:
        print("{}\t=>\t{}".format(
            entity["Type"], text[entity["BeginOffset"]:entity["EndOffset"]]))
        if entity["Type"] not in person_data:
            entity_type = entity["Type"]
            person_data[entity_type] = text[entity["BeginOffset"]                                            :entity["EndOffset"]]

    print(person_data)

    csv_file_path = os.path.join(os.getcwd(), 'resume_data', 'resume_data.csv')
    person_data_list = []
    if 'NAME' in person_data:
        person_data_list.append(person_data['NAME'])
    else:
        person_data_list.append("")
    if 'EMAIL' in person_data:
        person_data_list.append(person_data["EMAIL"])
    else:
        person_data_list.append("")
    if 'PHONE' in person_data:
        person_data_list.append(person_data["PHONE"])
    else:
        person_data_list.append("")
    if 'ADDRESS' in person_data:
        person_data_list.append(person_data["ADDRESS"])
    else:
        person_data_list.append("")
    print(person_data_list)
    with open(csv_file_path, 'a+') as f_object:
        writer_object = writer(f_object)
        writer_object.writerow(person_data_list)
        f_object.close()

    if(s3_client is not None and es_client is not None):
        if not es_client.check_if_doc_exists('resume_data.csv'):
            es_client.create_doc(file_id='1', file_name='resume_data.csv', created=' ',
                                 timestamp='', mimetype='text/csv', filetype='csv', user_id='admin', size='')
        response = s3_client.upload_file(
            csv_file_path, 'testbucket', 'resume_data.csv')


def main():
    file_name = 'resume.jpeg'

    # Creating an AWS Textract client
    textract_client = boto3.client('textract', aws_access_key_id=str(os.environ.get('AMAZON_AWS_ACCESS_KEY_ID')),
                                   aws_secret_access_key=str(os.environ.get('AMAZON_AWS_SECRET_ACCESS_KEY')))

    # Creating an AWS Comprehend client
    comprehend_client = boto3.client('comprehend', aws_access_key_id=str(os.environ.get('AMAZON_AWS_ACCESS_KEY_ID')),
                                     aws_secret_access_key=str(os.environ.get('AMAZON_AWS_SECRET_ACCESS_KEY')))

    process_resume(textract_client, comprehend_client, file_name)


if __name__ == "__main__":
    main()
