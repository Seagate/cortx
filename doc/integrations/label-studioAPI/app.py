import os
import json
import s3dataEndpoint
import labelStudioAPI
import streamlit as st
import xmltodict
from pathlib import Path
##########################################################################################################
token = '303a7e45277180b93567209aeca063088856ddf8'
lsAPI = labelStudioAPI.LabelStudioAPI(token)

END_POINT_URL = 'http://uvo100ebn7cuuq50c0t.vm.cld.sr'
A_KEY = 'AKIAtEpiGWUcQIelPRlD1Pi6xQ'
S_KEY = 'YNV6xS8lXnCTGSy1x2vGkmGnmdJbZSapNXaSaRhK'
##########################################################################################################
s3 = s3dataEndpoint.S3DataEndpoint(end_url=END_POINT_URL, accessKey=A_KEY, secretKey=S_KEY)

st.warning("Please don't leave any field blank, data points might get broken")
st.title("Welcome to Cortx Integration!")
# st.image('Images/opensource.jpg', width=600)

# Add a selectbox to the sidebar:
add_selectbox = st.sidebar.selectbox(
    'Create Project, Connect S3, Export',
    ('Project NEW, UPDATE or DELETE', 'Connect S3 storage', 'Upload dataset to S3',
     'Export Annotations', 'Download from S3 bucket')
)
st.write(add_selectbox)

if add_selectbox == 'Project NEW, UPDATE or DELETE':
    st.image('Images/dataForms.png')
    action = st.selectbox('Action', [ "create", "update", "delete" ])

    if action == 'delete':
        projid = st.text_input("Enter project ID to delete project")
        if st.button('Delete Project'):
            msg = lsAPI.Project(action='delete', projID=projid)

            if msg.status_code == 204:
                st.info("Label Studio Project deleted successfully")
                st.json(msg.text)
            else:
                st.info("Label Studio Project not deleted")

    elif action == 'create':
        xml = """"""
        projTitle = st.text_input("Enter your New Project Name", "")
        projDes = st.text_input("Enter description for project", "")
        uploaded_file = st.file_uploader('Upload Label Creator .XML file')
        if uploaded_file is not None:
            xml = uploaded_file.read()
            file1_data = json.loads(json.dumps(xmltodict.parse(xml)))
            st.write(file1_data)
            if st.button('Create NEW Project'):
                msg = lsAPI.Project(title=projTitle, description=projDes, labelXML=xml, action=action)
                if msg.status_code == 201:
                    st.info("Label Studio Project created successfully")
                    st.json(msg.text)
                else:
                    st.info("Label Studio Project could not be created")


elif add_selectbox == 'Connect S3 storage':
    st.image('Images/working.jpg')
    action = st.selectbox('Action', [ "Connect S3", "Sync S3", "Delete S3", "Create S3 Bucket" ])

    if action == "Connect S3":
        projid = st.text_input("Enter Label Studio Project ID", "")
        title = st.text_input("Enter a S3 storage title")
        bucket = st.text_input("Enter S3 bucket name", "testbucket")
        region = st.text_input("Enter S3 storage region", "US")
        accKey = st.text_input("Enter S3 Access Key", "AKIAtEpiGWUcQIelPRlD1Pi6xQ")
        secKey = st.text_input("Enter S3 Secret Key", "YNV6xS8lXnCTGSy1x2vGkmGnmdJbZSapNXaSaRhK")
        s3URL = st.text_input("Enter S3 URL", "http://uvo100ebn7cuuq50c0t.vm.cld.sr")

        if st.button('Connect'):
            msg = lsAPI.connectS3ProjStorage(projID=projid, title=title, bucket_name=bucket,
                                             region_name=region, accessKey=accKey, secretKey=secKey,
                                             s3_url=s3URL)
            if msg.status_code == 201:
                st.info("Cortx S3 connected to Label Studio successfully")
                st.json(msg.text)
            else:
                st.info("Cortx S3 not able to connect")

    elif action == "Delete S3":
        storid = st.text_input("Enter Label Studio S3 Storage ID", "")
        if st.button('Delete'):
            msg = lsAPI.delS3ProjBucket(storageID=storid)
            if msg.status_code == 204:
                st.info("Cortx S3 project storage deleted")
            else:
                st.info("Could not delete S3 project storage")

    elif action == "Sync S3":
        storid = st.text_input("Enter Label Studio S3 Storage ID", "")
        if st.button('Sync'):
            msg = lsAPI.syncS3Bucket(storageID=storid)
            if msg.status_code == 200:
                st.info("Synced all files from S3. Start labeling in Label Studio")
            else:
                st.info("Could not Sync from S3 for Storage ID" + storid)

    elif action == "Create S3 Bucket":
        bucketName = st.text_input("Enter name for your S3 bucket")
        if st.button("Create S3 Bucket"):
            if s3.bucket_operation(bucket_name=bucketName, operation='create'):
                print("S3 bucket made successfully!")

elif add_selectbox == 'Upload dataset to S3':
    st.image('Images/uploadCortx.jpg')

    st.beta_container()
    col1, col2 = st.beta_columns(2)

    with col1:
        datasetFolderPath = st.text_input("Enter dataset folder path")
        destS3Bucket = st.text_input("Enter destination S3 bucket name for file upload")
        if st.button('Upload'):
            for subdir, dirs, files in os.walk(datasetFolderPath):
                for file in files:
                    full_path = os.path.join(subdir, file)
                    with open(full_path, 'rb') as data:
                        if s3.file_operation(bucket_name=destS3Bucket, file_name=file,
                                             file_location=full_path,  operation='upload'):
                            st.info("Uploading file %s to S3 completed successfully!" % file)

    with col2:
        storid = st.text_input("Enter Label Studio S3 Storage ID", "")
        if st.button('Sync'):
            msg = lsAPI.syncS3Bucket(storageID=storid)
            if msg.status_code == 200:
                st.info("Synced all files from S3. Start labeling in Label Studio")
            else:
                st.info("Could not Sync from S3 for Storage ID" + storid)


elif add_selectbox == 'Export Annotations':
    st.image('Images/export.jpg')
    projid = st.text_input('Enter Project ID')
    export_bucket = st.text_input('Enter destination S3 bucket name')
    path_file_download = '/home/sumit/PycharmProjects/CortxProject/local/'

    st.beta_container()
    col1, col2, col3, col4 = st.beta_columns(4)

    with col1:
        if st.button('Export JSON'):
            file = lsAPI.exportAnnotations(projID=projid, exportFormat='JSON',
                                           exportPath=path_file_download)
            path_file_download = path_file_download + file
            if s3.file_operation(bucket_name=export_bucket, file_name=file,
                                 file_location=path_file_download, operation='upload'):
                st.info("Uploading file %s to S3 completed successfully!" % file)

    with col2:
        if st.button('Export CSV'):
            file = lsAPI.exportAnnotations(projID=projid, exportFormat='CSV',
                                           exportPath=path_file_download)
            path_file_download = path_file_download + file
            if s3.file_operation(bucket_name=export_bucket, file_name=file,
                                 file_location=path_file_download, operation='upload'):
                st.info("Uploading file %s to S3 completed successfully!" % file)
    with col3:
        if st.button('Export COCO'):
            file = lsAPI.exportAnnotations(projID=projid, exportFormat='COCO',
                                           exportPath=path_file_download)
            path_file_download = path_file_download + file

            if s3.file_operation(bucket_name=export_bucket, file_name=file,
                                 file_location=path_file_download, operation='upload'):
                st.info("Uploading file %s to S3 completed successfully!" % file)
    with col4:
        if st.button('PASCALVOC'):
            file = lsAPI.exportAnnotations(projID=projid, exportFormat='VOC',
                                           exportPath=path_file_download)
            path_file_download = path_file_download + file

            if s3.file_operation(bucket_name=export_bucket, file_name=file,
                                 file_location=path_file_download, operation='upload'):
                st.info("Uploading file %s to S3 completed successfully!" % file)

elif add_selectbox == 'Download from S3 bucket':
    st.image('Images/downloadS3.jpg')
    fileName = st.text_input("Enter name of the file to download")
    buckName = st.text_input("Enter S3 source bucket name")
    downloadPath = st.text_input("Enter downlaod path")
    if st.button("Download"):
        if s3.file_operation(bucket_name=buckName, file_name=fileName, file_location=downloadPath,
                             operation='download'):
            print("Downloading the file to S3 has been completed successfully!")





