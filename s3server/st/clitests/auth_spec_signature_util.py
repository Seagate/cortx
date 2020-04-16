import os
import sys
import string
import hmac
import base64
import datetime
import hashlib
from hashlib import sha1, sha256
from s3client_config import S3ClientConfig


class GlobalCredentials():
    root_access_key = ""
    root_secret_key = ""

# Fetch access_key and secret_key
def _use_root_credentials():
    credential_file = os.path.join(os.path.dirname(__file__),'aws_credential_file')
    f = open(credential_file, "r")
    content = f.read()
    # Iterate over credential file
    for line in content.splitlines():
        if 'aws_access_key_id =' in line:
           GlobalCredentials.root_access_key = line.split('=')[1].strip()
        if 'aws_secret_access_key =' in line:
           GlobalCredentials.root_secret_key = line.split('=')[1].strip()
    f.close()

def utf8_encode(msg):
    return msg.encode('UTF-8')

def utf8_decode(msg):
    return str(msg, 'UTF-8')

# if x-amz-* has multiple values then value for that header should be passed as
# list of values eg. headers['x-amz-authors'] = ['Jack', 'Jelly']
# example return value: x-amz-authors:Jack,Jelly\nx-amz-org:Seagate\n
def _get_canonicalized_xamz_headers(headers):
    xamz_headers = ''
    for header in sorted(headers.keys()):
        if header.startswith("x-amz-"):
            if type(headers[header]) is str:
                xamz_headers += header + ":" + headers[header] + "\n"
            elif type(headers[header]) is list:
                xamz_headers += header + ":" + ','.join(headers[header]) + "\n"

    return xamz_headers

# fetch bucketname and query param
# return value : /<bucket-name>/?<query-params>
def _get_canonicalized_resource(canonical_uri, params):
    # Extract bucket name from host entry
    bucket_name = params['Host'].split('.')[0]
    canonicalized_resource = canonical_uri + bucket_name + '/?' + params['ClientQueryParams']
    return canonicalized_resource

def _create_str_to_sign(http_method, canonical_uri, headers):
    str_to_sign = http_method + '\n'

    content_md5_value = headers.get("Content-MD5")
    if content_md5_value == None :
       str_to_sign += "" + "\n"
    else :
       str_to_sign += content_md5_value + "\n"

    content_type_value = headers.get("Content-Type")
    if content_type_value == None :
       str_to_sign += "" + "\n"
    else :
       str_to_sign += content_type_value + "\n"

    date_value = headers.get("Date")
    if date_value == None :
       str_to_sign += "" + "\n"
    else :
       str_to_sign += date_value + "\n"

    str_to_sign += _get_canonicalized_xamz_headers(headers)

    canonicalized_resource = _get_canonicalized_resource(canonical_uri, headers)
    str_to_sign += canonicalized_resource
    str_to_sign = utf8_encode(str_to_sign)

    return str_to_sign

def sign_request_v2(method, canonical_uri, headers):
    _use_root_credentials()
    str_to_sign = _create_str_to_sign(method, canonical_uri, headers)
    signature = utf8_decode(base64.encodestring(
        hmac.new(utf8_encode(GlobalCredentials.root_secret_key), str_to_sign, sha1).digest()).strip())
    auth_header = "AWS %s:%s" % (GlobalCredentials.root_access_key, signature)

    return auth_header

def create_canonical_request(method, canonical_uri, body, epoch_t, host, signed_headers, request_params):
    canonical_query_string = "";
    canonical_headers="";

    for header in signed_headers.split(';'):
        value=""
        # Use current datetime for signature calculation
        if header == "x-amz-date":
           canonical_headers = canonical_headers + header.lower()+':'+get_timestamp(epoch_t)+'\n'
           continue;
        # Fetch Content-MD5 value from request parameters
        if header == "content-md5":
           canonical_headers = canonical_headers + header.lower()+':'+request_params['Content-MD5']+'\n'
           continue;

        # Fetch header value
        if header in request_params:
           value = request_params[header]
        elif header.title() in request_params:
           value = request_params[header.title()]

        # Handle parsing integer value for 'Content-Length'
        if isinstance(value, int):
           canonical_headers = canonical_headers + header.lower()+':'+str(value)+'\n'
        else:
           canonical_headers = canonical_headers + header.lower()+':'+value+'\n'

    if "x-amz-content-sha256" in request_params:
       payload_hash = request_params['x-amz-content-sha256']
    else :
       payload_hash = hashlib.sha256(body.encode('utf-8')).hexdigest()

    canonical_request = method + '\n' + canonical_uri + '\n' + canonical_query_string + '\n' + \
        canonical_headers + '\n' + signed_headers + '\n' + payload_hash

    return canonical_request

def sign(key, msg):
    return hmac.new(key, msg.encode('utf-8'), hashlib.sha256).digest()

def getV4SignatureKey(key, dateStamp, regionName, serviceName):
    kDate = sign(('AWS4' + key).encode('utf-8'), dateStamp)
    kRegion = sign(kDate, regionName)
    kService = sign(kRegion, serviceName)
    kSigning = sign(kService, 'aws4_request')

    return kSigning

def create_string_to_sign_v4(method='', canonical_uri='', body='', epoch_t='',
        algorithm='', host='' , service='', region='', signed_headers='', request_params=''):

    canonical_request = create_canonical_request(method, canonical_uri,
        body, epoch_t, host, signed_headers, request_params);

    credential_scope = get_date(epoch_t) + '/' + region + '/' + service + '/' + 'aws4_request'

    string_to_sign = algorithm + '\n' + get_timestamp(epoch_t) + '\n' + credential_scope \
    + '\n' +  hashlib.sha256(canonical_request.encode('utf-8')).hexdigest();

    return string_to_sign

def sign_request_v4(method=None, canonical_uri='/', body='', epoch_t='',
        host='', service='', region='', signed_headers='', request_params=None):

    _use_root_credentials()
    if method is None:
        print("method can not be null")
        return None
    if request_params is None:
        print("request parameters can not be null")
        return None

    credential_scope = get_date(epoch_t) + '/' + region + \
        '/' + service + '/' + 'aws4_request'

    algorithm = 'AWS4-HMAC-SHA256';

    string_to_sign = create_string_to_sign_v4(method, canonical_uri, body, epoch_t,
          algorithm, host, service, region, signed_headers, request_params)

    signing_key = getV4SignatureKey(GlobalCredentials.root_secret_key, get_date(epoch_t), region, service);

    signature = hmac.new(signing_key, (string_to_sign).encode('utf-8'), hashlib.sha256).hexdigest();

    authorization_header = algorithm + ' ' + 'Credential=' + GlobalCredentials.root_access_key + '/' + \
        credential_scope + ', ' +'SignedHeaders=' + signed_headers + \
        ', ' +  'Signature=' + signature

    return authorization_header

def get_date(epoch_t):
    return epoch_t.strftime('%Y%m%d')

def get_timestamp(epoch_t):
    return epoch_t.strftime('%Y%m%dT%H%M%SZ')

