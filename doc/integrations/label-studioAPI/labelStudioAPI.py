import requests
import json
from pathlib import Path

# url = 'http://localhost:8080/api/Cortx/1/tasks/bulk/'
# token = '303a7e45277180b93567209aeca063088856ddf8'

class LabelStudioAPI:
    def __init__(self, token, debug=True):
        self.token = token
        self.debug = debug

    def Project(self, title='Demo', labelXML="""""", description='New Task', action='create', projID='1'):
        if action == 'update':
            url = 'http://localhost:8080/api/projects/'
            headers = {'Authorization': 'Token ' + self.token}
            payload = {'title': title, 'description': description, 'label_config': labelXML}
            res = requests.patch(url, data=payload, headers=headers)
            print(res.status_code, res.content)

        elif action == 'create':
            url = 'http://localhost:8080/api/projects/'
            headers = {'Authorization': 'Token ' + self.token}
            payload = {'title': title, 'description': description, 'label_config': labelXML}
            res = requests.post(url, data=payload, headers=headers)
            print(res.status_code, res.content)
            if res.status_code == 201:
                print('Successfully created NEW Project ' + title)
                return res
            else:
                print('Could not create NEW project')
                return res

        elif action == 'delete':
            url = 'http://localhost:8080/api/projects/' + projID + '/'
            headers = {'Authorization': 'Token ' + self.token}
            res = requests.delete(url, headers=headers)
            print(res.status_code, res.content)
            if res.status_code == 204:
                print('Successfully deleted Project ' + projID)
                return res
            else:
                print('Could not delete')
                return res
        else:
            print("Not valid action")

    def connectS3ProjStorage(self, projID="1", title="S3", bucket_name="", region_name="US", accessKey="",
                         secretKey="", s3_url=""):
        url = 'http://localhost:8080/api/storages/s3'
        headers = {'Authorization': 'Token ' + self.token}

        payload = {"project": projID, "title": title, "bucket": bucket_name, "region_name": region_name,
                   "s3_endpoint": s3_url, "aws_access_key_id": accessKey, "aws_secret_access_key": secretKey,
                   "use_blob_urls": True, "presign_ttl": "1"}

        res = requests.post(url, data=payload, headers=headers)
        print(res.status_code)

        if res.status_code == 201:
            print('S3 connected')
            return res
        else:
            print('Could not connect S3')
            return res

        print("Sync S3 bucket to see all your data in label studio")

    def delS3ProjBucket(self, storageID='1'):
        url = 'http://localhost:8080/api/storages/s3/' + storageID
        headers = {'Authorization': 'Token ' + self.token}
        # payload = {}
        res = requests.delete(url, headers=headers)
        print(res.status_code)
        return res

    def syncS3Bucket(self, storageID='1'):
        url = 'http://localhost:8080/api/storages/s3/' + storageID + '/sync'
        headers = {'Authorization': 'Token ' + self.token}
        # payload = {}
        res = requests.post(url, headers=headers)
        print(res.status_code)

        return res

    # export Annotations in all kinds of widely accepted data annotation formats,
    # JSON, CSV, COCO, PASCAL VOC (VOC)

    def exportAnnotations(self, projID='1', exportFormat='JSON',
                          exportPath='/home/sumit/PycharmProjects/CortxProject/local/'):

        folder = Path(exportPath)
        url = 'http://localhost:8080/api/projects/' + projID + '/export?exportType=' + exportFormat
        headers = {'Authorization': 'Token ' + self.token}
        res = requests.get(url, headers=headers)
        print(res.content)

        if exportFormat == 'JSON':
            python_data = json.loads(res.text)
            object_name = ("annotations" + projID + ".json")
            file_name = folder / ("annotations" + projID + ".json")
            with open(file_name, 'w') as responseFile:
                json.dump({'Data': python_data}, responseFile)
            print("JSON data annotation local export completed.")
            return object_name

        elif exportFormat == 'CSV':
            object_name = ("annotations" + projID + ".csv")
            file_name = folder / ('annotations' + projID + '.csv')
            f = open(file_name, "w")
            f.write(res.text)
            f.close()
            print("CSV data annotation local export completed")
            return object_name

        elif exportFormat == 'COCO':
            object_name = ("annotationsCOCO" + projID + ".zip")
            file_name = folder / ('annotationsCOCO' + projID + '.zip')
            with open(file_name, 'wb') as out_file:
                out_file.write(res.content)
            print("COCO data annotation local export completed")
            return object_name

        elif exportFormat == 'VOC':
            object_name = ("annotationsPASCAL" + projID + ".zip")
            file_name = folder / ('annotationsPASCAL' + projID + '.zip')
            with open(file_name, 'wb') as out_file:
                out_file.write(res.content)
            print("PASCAL data annotation local export completed")
            return object_name

        else:
            print("Not supported export format, currently supported are JSON,CSV,COCO,PASCAL")


def main():
    token = '303a7e45277180b93567209aeca063088856ddf8'
    a = LabelStudioAPI(token=token)

    # loading the xml file
    with open('label_ceate.xml', 'r') as f:
        label_create = f.read()
        # print(f.read())

    a.makeProject(title='Hello', labelXML=label_create)


if __name__ == "__main__":
    main()
