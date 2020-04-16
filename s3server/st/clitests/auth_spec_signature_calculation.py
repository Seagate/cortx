import os
import yaml
import pprint
import http.client, urllib.parse
import ssl
import json
import xmltodict
import re
from framework import Config
from s3client_config import S3ClientConfig
from datetime import datetime
from auth_spec_signature_util import sign_request_v4
from auth_spec_signature_util import sign_request_v2
from auth_spec_signature_util import get_timestamp

class AuthHTTPClient:
    def __init__(self):
        self.iam_uri_https = S3ClientConfig.iam_uri_https
        self.iam_uri_http = S3ClientConfig.iam_uri_http

    def authenticate_user(self, headers, body):
        if Config.no_ssl:
            conn = http.client.HTTPConnection(urllib.parse.urlparse(self.iam_uri_http).netloc)
        else:
            conn = http.client.HTTPSConnection(urllib.parse.urlparse(self.iam_uri_https).netloc,
                                           context= ssl._create_unverified_context())
        conn.request("POST", "/", urllib.parse.urlencode(body), headers)
        response_data = conn.getresponse().read()
        conn.close()
        return response_data

def check_response(expected_response, test_response):
    #assert test_response.decode("utf-8") in expected_response
    #SignatureSHA256, Request id is dynamically generated, so compare string
    #skipping SignatureSHA256, request id
    if "<SignatureSHA256>" in expected_response:
       expected = expected_response.split("<SignatureSHA256>")
    else:
       expected = expected_response.split("<RequestId>")

    assert expected[0] in test_response.decode("utf-8")
    print("Response has [%s]." % (test_response))

def update_signature_headers(params):
    # Update Signature value
    if 'Authorization' in params :
        origional_headers = params['Authorization']
        # Split by space
        authorization_headers = origional_headers.split()
        # Handle aws v4 signature
        if 'AWS4-HMAC-SHA256' in authorization_headers:
            # Generate Signature header
            # Get host value from url https://iam.seagate.com:9443
            if Config.no_ssl:
               url_parse_result = urllib.parse.urlparse(S3ClientConfig.iam_uri_http).netloc
            else:
               url_parse_result = urllib.parse.urlparse(S3ClientConfig.iam_uri_https).netloc

            epoch_t = datetime.utcnow();
            body = urllib.parse.urlencode({'Action':'%s'},params['Action'])
            # Fetch signed_headers string from Authorization header
            signed_headers = authorization_headers[2].split('=')[1].split(',')[0]

            authroization_value = sign_request_v4(params['Method'], params['ClientAbsoluteUri'], body, epoch_t,
            url_parse_result, "s3", "seagate", signed_headers, params)
            # Update X-Amz-Date value with current timestamp
            if 'X-Amz-Date' in params:
                params['X-Amz-Date'] = get_timestamp(epoch_t)
            params['Authorization'] = authroization_value

        # Handle aws v2 signature
        elif 'AWS' in authorization_headers:
              # Update date with current timestamp
              if 'Date' in params:
                  params['Date'] = _get_date_utc_timestamp()
              # Calculate Authorization header value
              authroization_value = sign_request_v2(params['Method'], params['ClientAbsoluteUri'], params)
              params['Authorization'] = authroization_value

    return params

# Get current date in GMT format e.g. Thu, 05 Jul 2018 03:55:43 GMT
def _get_date_utc_timestamp():
    epoch_t = datetime.utcnow()
    return epoch_t.strftime('%a, %d %b %Y %H:%M:%S %ZGMT')

test_data = {}
test_data_file = os.path.join(os.path.dirname(__file__), 'auth_spec_signcalc_test_data.yaml')
with open(test_data_file, 'r') as f:
    test_data = yaml.safe_load(f)

for test in test_data:
    print("Test case [%s] - " % test_data[test]['test-title'])
    headers = test_data[test]['req-headers']
    params = test_data[test]['req-params']

    # Update Authroization header with current date timestamp
    updated_params = update_signature_headers(params)
    expected_response = test_data[test]['output']
    test_response = AuthHTTPClient().authenticate_user(headers, updated_params)

    check_response(expected_response, test_response)
    print("Test was successful\n")

