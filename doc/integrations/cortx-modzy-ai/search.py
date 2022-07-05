import boto3
from botocore.exceptions import ClientError
from botocore.client import Config
from modzy import ApiClient
from pdf2image import convert_from_bytes
import streamlit as st
import base64
import uuid
import re
from weasyprint import HTML
import json
import numpy as np
import pandas as pd
from collections import Counter

input_config = 'config.json'

colors = ['#7aecec', '#aa9cfc', '#feca74', '#bfe1d9', '#c887fb', '#e4e7d2',
          '#905829', '#dfcd62', '#e8d53d', '#f0c88b', '#b68282', '#799fb2',
          '#c3b489', '#bf9a81', '#a592ae', '#e5aef9', '#f69419', '#a36b42',
          '#e5c3a6', '#4fc6b4', '#d9e69a', '#f76b6b', '#e8d53d', '#61c861',
          '#65a4d9', '#b8ff57', '#779987', '#f69419']

st.set_page_config(
    page_title="Object Storage Statistical Scanner App",
    page_icon=":shark:",
    layout="wide",
    initial_sidebar_state="auto",
    menu_items={
        'About': "# S3 Statistical File Scanner. This is an *extremely* cool app!"
    }
)

st.title("Cortx Statistical File Scanner")
st.sidebar.write("Enter Cortx S3 Credentials")
form = st.sidebar.form("aws_credentials")

if 'job_ids' not in st.session_state:
    st.session_state.job_ids = []

if 'person_entity_data' not in st.session_state:
    st.session_state.person_entity_data = []

if 'location_entity_data' not in st.session_state:
    st.session_state.location_entity_data = []

if 'organization_entity_data' not in st.session_state:
    st.session_state.organization_entity_data = []

if 'tag_data' not in st.session_state:
    st.session_state.tag_data = []

if 'summary_data' not in st.session_state:
    st.session_state.summary_data = []

if 'file_data_tags' not in st.session_state:
    st.session_state.file_data_tags = dict()

if 'file_data_entities' not in st.session_state:
    st.session_state.file_data_entities = dict()

if 'file_data_summary' not in st.session_state:
    st.session_state.file_data_summary = dict()

if 'full_report_download_link' not in st.session_state:
    st.session_state.full_report_download_link = None

@st.cache
def beautify_tags(tags):
    """
    Function to beautify tags result
    Args:
        tags: list of tags
    """

    div = '<div class="entities" style="line-height: 2.5; direction: ltr">'
    mark = '<mark class="entity" style="background:{}; padding: 0.45em 0.6em; margin: 0 0.25em; line-height: 1; border-radius: 0.35em;">{}\n<span style="font-size: 0.8em; font-weight: bold; line-height: 1; border-radius: 0.35em; text-transform: uppercase; vertical-align: middle; margin-left: 0.5rem"></span></mark>'

    html = "" + div

    for i in range(len(tags[:5])):
        html += mark.format("#F63366", tags[i])
    html += '</div>'

    html += div

    for i in range(5, len(tags)):
        html += mark.format("#F63366", tags[i])
    html += '</div>'

    return html


@st.cache
def beautify_html(html: str, title=False, title_string=""):
    """
    Function to display border.
    Args:
        html: html string to beautify
        title: bool
        title_string: string that appears as header
    """

    if title:
        WRAPPER = """<div style="border: 1px solid #e6e9ef; border-radius: 0.25rem; padding: 1rem; margin-bottom: 2.5rem"><header><h3 style="text-decoration: underline;">{}<h3></header>{}</div>"""
        html = html.replace("\n", " ")
        return WRAPPER.format(title_string, html)
    else:
        WRAPPER = """<div style="border: 1px solid #e6e9ef; border-radius: 0.25rem; padding: 1rem; margin-bottom: 2.5rem">{}</div>"""
        html = html.replace("\n", " ")
        return WRAPPER.format(html)


@st.cache
def beautify_entities(response):
    """
    Function to beautify entities result
    Args:
        response: entity response from Modzy
    """

    unique_entities = list(set([tag[1] for tag in response]))

    entity_color = dict()

    for i in range(len(unique_entities)):
        entity_color[unique_entities[i]] = colors[i]

    div1 = '<div class="entities" style="line-height: 2.5; direction: ltr">'
    mark1 = '<mark class="entity" style="background:'
    mark2 = '; padding: 0.45em 0.6em; margin: 0 0.25em; line-height: 1; border-radius: 0.35em;">'
    sp1 = '<span style="font-size: 0.8em; font-weight: bold; line-height: 1; border-radius: 0.35em; text-transform: uppercase; vertical-align: middle; margin-left: 0.5rem">'
    e1 = '</span></mark>'

    html = ""
    html += div1

    for i in range(len(response)):
        if response[i][1] == "O":
            html += response[i][0] + " "
        else:
            html += mark1 + entity_color[response[i][1]] + mark2 + response[i][0] + sp1 + response[i][1] + e1

    html += '</div>'

    return html


def download_file(object_to_download, name):
    """
	Function to download visualized result to as a .pdf file.
	Args:
		object_to_download: object/var to download
		name: name to give to downloaded file
	"""
    try:
        b64 = base64.b64encode(object_to_download.encode()).decode()

    except AttributeError as e:
        b64 = base64.b64encode(object_to_download).decode()

    button_text = 'Save Result'
    button_uuid = str(uuid.uuid4()).replace('-', '')
    button_id = re.sub('\d+', '', button_uuid)

    custom_css = f""" 
		<style>
			#{button_id} {{
				background-color: rgb(255, 255, 255);
				color: rgb(38, 39, 48);
				padding: 0.25em 0.38em;
				position: relative;
				text-decoration: none;
				border-radius: 4px;
				border-width: 1px;
				border-style: solid;
				border-color: rgb(230, 234, 241);
				border-image: initial;
			}} 
			#{button_id}:hover {{
				border-color: rgb(246, 51, 102);
				color: rgb(246, 51, 102);
			}}
			#{button_id}:active {{
				box-shadow: none;
				background-color: rgb(246, 51, 102);
				color: white;
				}}
		</style> """

    dl_link = custom_css + f'<a download="{name}" id="{button_id}" href="data:file/txt;base64,{b64}">{button_text}</a><br></br>'

    return dl_link


def load_config():
    """
	Load Modzy API configurations from config.json file
	"""
    with open('api_config.json') as f:
        return json.load(f)


@st.cache(allow_output_mutation=True)
def get_client():
    """
	Function to get modzy client
	"""
    config = load_config()

    if not config["API_URL"] and config["API_KEY"]:
        st.error("Please update api_config.json file with valid credentials...")
    else:
        try:
            client = ApiClient(base_url=config["API_URL"], api_key=config["API_KEY"])
        except:
            st.error("Couldn't connect to Modzy server, please verify credentials...")

    return client


@st.cache
def get_model_output(client, model_identifier, model_version, data_sources, explain=False):
    """
	Args:
		client: modzy client object
		model_identifier: model identifier (string)
		model_version: model version (string)
		data_sources: dictionary with the appropriate filename --> local file key-value pairs
		explain: boolean variable, defaults to False. If true, model will return explainable result
	"""
    client = get_client()
    if model_identifier == "c60c8dbd79":
        job = client.jobs.submit_files_bulk(model_identifier, model_version, data_sources)
    else:
        job = client.jobs.submit_text(model_identifier, model_version, data_sources, explain)
    result = client.results.block_until_complete(job, timeout=None)
    model_output = result.get_first_outputs()['results.json']

    st.session_state.job_ids.append(job.jobIdentifier)

    return model_output


@st.cache
def get_model(client, model_name):
    """
	Function to load model based on name
	Args:
		client: modzy client object
		model_name: model name to load
	"""
    model = client.models.get_by_name(model_name)
    modelVersion = client.models.get_version(model, model.latest_version)

    return model, modelVersion.version


def prepare_statistical_summary(s3_resource, s3_client, bucket_name, client):
    objects_response = s3_client.list_objects(Bucket=bucket_name)

    try:
        if objects_response['Contents']:
            for object in objects_response['Contents']:
                key = object['Key']

                # for pdf's do
                if key.endswith('.pdf'):
                    image_files = []

                    content = s3_resource.Object(bucket_name, object['Key']).get()['Body'].read()
                    images = convert_from_bytes(content, fmt='png')
                    key = key.replace('.pdf', '')

                    for image in images:
                        image.save('data/images/' + str(key) + '_page' + str(
                            images.index(image)) + '.jpg', 'JPEG')
                        image_files.append('data/images/' + str(key) + '_page' + str(
                            images.index(image)) + '.jpg')

                    sources = {}
                    for page in image_files:
                        sources['page' + str(image_files.index(page))] = {
                            'input': page,
                            'config.json': input_config
                        }

                    # convert pdf documents to images for OCR
                    model, model_version = get_model(client, "Multi-language OCR")
                    ocr_output = get_model_output(client, model.modelId, model_version, sources, explain=False)
                    ocr_text = beautify_html(ocr_output["text"], title=True, title_string="OCR")
                    # st.write("")
                    # st.markdown(ocr_text, unsafe_allow_html=True)

                    # extract topics from OCR texts
                    model, model_version = get_model(client, "Text Topic Modeling")
                    sources = {"source-key": {"input.txt": ocr_text}}
                    text_topic_result = get_model_output(client, model.modelId, model_version, sources, explain=False)
                    st.session_state.tag_data.append(text_topic_result)
                    tags = beautify_html(beautify_tags(text_topic_result), title=True, title_string="Tags")
                    st.session_state.file_data_tags[object['Key']] = tags
                    # st.write("")
                    # st.markdown(tags, unsafe_allow_html=True)

                    # Generate summary from OCR text
                    model, model_version = get_model(client, "Text Summarization")
                    sources = {"source-key": {"input.txt": ocr_text}}
                    text_summarization_result = get_model_output(client, model.modelId, model_version, sources,
                                                                 explain=False)
                    st.session_state.summary_data.append(text_summarization_result["summary"])
                    summary = beautify_html(text_summarization_result["summary"], title=True, title_string="Summary")
                    st.session_state.file_data_summary[object['Key']] = summary
                    # st.write("")
                    # st.markdown(summary, unsafe_allow_html=True)

                    # Named Entity Recognition from OCR text
                    modelId, model_version = "a92fc413b5", "0.0.12"
                    sources = {"source-key": {"input.txt": ocr_text}}
                    named_entity_recognition_result = get_model_output(client, modelId, model_version, sources,
                                                                       explain=False)
                    # find B-PER, I-PER, B-LOC, I-LOC, B-ORG, I-ORG, remove duplicate entities
                    persons = list(set([tag[0] for tag in named_entity_recognition_result if
                                        tag[1] == 'B-PER' or tag[1] == 'I-PER']))
                    locations = list(set([tag[0] for tag in named_entity_recognition_result if
                                          tag[1] == 'B-LOC' or tag[1] == 'I-LOC']))
                    organizations = list(set([tag[0] for tag in named_entity_recognition_result if
                                              tag[1] == 'B-ORG' or tag[1] == 'I-ORG']))

                    st.session_state.person_entity_data.append(persons)
                    st.session_state.location_entity_data.append(locations)
                    st.session_state.organization_entity_data.append(organizations)

                    # st.write("")
                    entities = beautify_html(beautify_entities(named_entity_recognition_result), title=True, title_string="Entities")
                    st.session_state.file_data_entities[object['Key']] = entities
                    # download report link
                    pdfile = HTML(string=tags + summary + beautify_html(entities, title=True,
                                                                        title_string="Entities")).write_pdf()
                    download_button_str = download_file(pdfile, f'Summary_%s.pdf' % key)
                    st.markdown(download_button_str, unsafe_allow_html=True)

                # for text files do
                if key.endswith('.txt'):
                    body = s3_resource.Object(bucket_name, object['Key']).get()['Body'].read()

                    # extract topics from texts
                    model, model_version = get_model(client, "Text Topic Modeling")
                    sources = {"source-key": {"input.txt": body.decode("utf-8")}}
                    text_topic_result = get_model_output(client, model.modelId, model_version, sources, explain=False)
                    st.session_state.tag_data.append(text_topic_result)
                    tags = beautify_html(beautify_tags(text_topic_result), title=True, title_string="Tags")
                    st.session_state.file_data_tags[object['Key']] = tags
                    # st.write("")
                    # st.markdown(tags, unsafe_allow_html=True)

                    # Generate summary from text
                    model, model_version = get_model(client, "Text Summarization")
                    sources = {"source-key": {"input.txt": body.decode("utf-8")}}
                    text_summarization_result = get_model_output(client, model.modelId, model_version, sources,
                                                                 explain=False)
                    st.session_state.summary_data.append(text_summarization_result["summary"])
                    summary = beautify_html(text_summarization_result["summary"], title=True, title_string="Summary")
                    st.session_state.file_data_summary[object['Key']] = summary
                    # st.write("")
                    # st.markdown(summary, unsafe_allow_html=True)

                    # Named Entity Recognition from text
                    modelId, model_version = "a92fc413b5", "0.0.12"
                    sources = {"source-key": {"input.txt": body.decode("utf-8")}}
                    named_entity_recognition_result = get_model_output(client, modelId, model_version, sources,
                                                                       explain=False)
                    # find B-PER, I-PER, B-LOC, I-LOC, B-ORG, I-ORG, remove duplicate entities
                    persons = list(set([tag[0] for tag in named_entity_recognition_result if
                                        tag[1] == 'B-PER' or tag[1] == 'I-PER']))
                    locations = list(set([tag[0] for tag in named_entity_recognition_result if
                                          tag[1] == 'B-LOC' or tag[1] == 'I-LOC']))
                    organizations = list(set([tag[0] for tag in named_entity_recognition_result if
                                              tag[1] == 'B-ORG' or tag[1] == 'I-ORG']))

                    st.session_state.person_entity_data.append(persons)
                    st.session_state.location_entity_data.append(locations)
                    st.session_state.organization_entity_data.append(organizations)

                    # st.write("")
                    entities = beautify_html(beautify_entities(named_entity_recognition_result), title=True, title_string="Entities")
                    st.session_state.file_data_entities[object['Key']] = entities

                    # download report
                    pdfile = HTML(string=tags + summary + beautify_html(entities, title=True,
                                                                        title_string="Entities")).write_pdf()
                    download_button_str = download_file(pdfile, f'Summary_%s.pdf' % key)
                    st.markdown(download_button_str, unsafe_allow_html=True)



    except ClientError as e:
        st.error("Enter correct credentials")
        if e.response['Error']['Code'] == 'InvalidAccessKeyId':
            print("Invalid Access Key ID")
        elif e.response['Error']['Code'] == 'InvalidSecurity':
            print("Invalid Secret Access Key")
        elif e.response['Error']['Code'] == 'AccessDenied':
            print("Access Denied")
        else:
            print("Unknown Error")

def plot_summary():
    tagCounter = Counter(flatten_list(st.session_state.tag_data))
    tag_chart_data = pd.DataFrame.from_dict(tagCounter, orient='index', columns=['Tags'])
    st.bar_chart(tag_chart_data)

    personCounter = Counter(flatten_list(st.session_state.person_entity_data))
    person_chart_data = pd.DataFrame.from_dict(personCounter, orient='index', columns=["People"])
    st.bar_chart(person_chart_data)

    organizationCounter = Counter(flatten_list(st.session_state.organization_entity_data))
    org_chart_data = pd.DataFrame.from_dict(organizationCounter, orient='index', columns=["Organizations"])
    st.bar_chart(org_chart_data)

    locationCounter = Counter(flatten_list(st.session_state.location_entity_data))
    location_chart_data = pd.DataFrame.from_dict(locationCounter, orient='index', columns=["Locations"])
    st.bar_chart(location_chart_data)



def flatten_list(t):
    return [item for sublist in t for item in sublist]


def get_credentials():
    access_key = form.text_input('S3 Access Key ID')
    secret_key = form.text_input('S3 Secret Access Key')
    region = form.text_input('Region')
    endpoint = form.text_input('Endpoint')
    submitted = form.form_submit_button("Submit")

    if submitted:
        access_key = access_key.strip()
        secret_key = secret_key.strip()
        region = region.strip()
        endpoint = endpoint.strip()

    elif access_key == '' or secret_key == '' or region == '' or endpoint == '':
        # st.warning("Please enter correct credentials")
        access_key = 'sgiamadmin'
        secret_key = 'ldapadmin'
        region = 'None'
        endpoint = 'http://192.168.1.14:31949'

    s3_resource = boto3.resource('s3', endpoint_url=endpoint,
                                 aws_access_key_id=access_key,
                                 aws_secret_access_key=secret_key,
                                 verify=False)

    s3_client = boto3.client('s3', endpoint_url=endpoint,
                             aws_access_key_id=access_key,
                             aws_secret_access_key=secret_key,
                             verify=False)

    return s3_resource, s3_client


@st.experimental_memo(suppress_st_warning=True)
def select_bucket(_s3_client):
    bucket_response = s3_client.list_buckets()
    # st.json(bucket_response)
    buckets = []

    try:
        if bucket_response['Buckets']:
            for bucket in bucket_response['Buckets']:
                buckets.append(bucket['Name'])

        return buckets, len(bucket_response['Buckets'])

    except ClientError as e:
        st.error("Enter correct credentials")
        if e.response['Error']['Code'] == 'InvalidAccessKeyId':
            print("Invalid Access Key ID")
        elif e.response['Error']['Code'] == 'InvalidSecurity':
            print("Invalid Secret Access Key")
        elif e.response['Error']['Code'] == 'AccessDenied':
            print("Access Denied")
        else:
            print("Unknown Error")


@st.experimental_memo(suppress_st_warning=True)
def select_bucket_objects(_s3_client, bucket_name):
    objects_response = s3_client.list_objects(Bucket=bucket_name)
    objects = []

    try:
        if objects_response['Contents']:
            for object in objects_response['Contents']:
                objects.append(object['Key'])

        return objects

    except ClientError as e:
        st.error("Enter correct credentials")
        if e.response['Error']['Code'] == 'InvalidAccessKeyId':
            print("Invalid Access Key ID")
        elif e.response['Error']['Code'] == 'InvalidSecurity':
            print("Invalid Secret Access Key")
        elif e.response['Error']['Code'] == 'AccessDenied':
            print("Access Denied")
        else:
            print("Unknown Error")


@st.experimental_memo(suppress_st_warning=True)
def list_files(_s3_client, _s3_resource, bucket_name):
    size = 0
    try:
        list_response = s3_client.list_objects_v2(Bucket=bucket_name)
        # text = ''
        # for page in pdf.pages:
        #     text + page.extractText()
        # st.write(text)

        if list_response['KeyCount'] == 0:
            st.warning("No files found")
            return list_response['KeyCount'], size
        else:
            for i in list_response.get('Contents'):
                size += i.get('Size')
            return list_response['KeyCount'], size

    except ClientError as e:
        st.error("Enter correct credentials")
        if e.response['Error']['Code'] == 'InvalidAccessKeyId':
            print("Invalid Access Key ID")
        elif e.response['Error']['Code'] == 'InvalidSecurity':
            print("Invalid Secret Access Key")
        elif e.response['Error']['Code'] == 'AccessDenied':
            print("Access Denied")
        else:
            print("Unknown Error")


s3_resource, s3_client = get_credentials()

file_count, size, bucket_count = 0, 0, 0

buckets, bucket_count = select_bucket(s3_client)

bucket_option = st.selectbox("Select Bucket", buckets)
file_count, size = list_files(s3_client, s3_resource, bucket_option)

col1, col2, col3 = st.columns(3)
col1.metric('Total S3 Buckets', value=bucket_count, delta=bucket_count)
col2.metric('Total Files', value=file_count, delta=file_count)
col3.metric('Total Size (Bytes)', value=size, delta=size)

client = get_client()
ocr = st.button("Start Statistical Analyzer")

plot_summary()

select_bucket_objects(s3_client, 'test')

if ocr:
    prepare_statistical_summary(s3_resource, s3_client, bucket_option, client)


report_option = st.sidebar.radio("Select to view individual file report", ('None', 'Topics', 'Entity', 'Summary'))

container = st.container()
if report_option == 'Topics':
    file = st.sidebar.selectbox("Select file", select_bucket_objects(s3_client, bucket_option))
    container.markdown(st.session_state.file_data_tags[file], unsafe_allow_html=True)

elif report_option == 'Entity':
    file = st.sidebar.selectbox("Select file", select_bucket_objects(s3_client, bucket_option))
    container.markdown(st.session_state.file_data_entities[file], unsafe_allow_html=True)

elif report_option == 'Summary':
    file = st.sidebar.selectbox("Select file", select_bucket_objects(s3_client, bucket_option))
    container.markdown(st.session_state.file_data_summary[file], unsafe_allow_html=True)

else:
    container.empty()
