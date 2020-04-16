import sys
import string
import hmac
import base64
import datetime
import hashlib
from hashlib import sha1, sha256
from s3iamcli.config import Credentials

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

def _get_canonicalized_resource(canonical_uri, params):
    # TODO: if required
    pass


def _create_str_to_sign(http_method, canonical_uri, params, headers):
    str_to_sign = http_method + '\n'
    str_to_sign += headers.get("content-md5", "") + "\n"
    str_to_sign += headers.get("content-type", "") + "\n"
    str_to_sign += headers.get("date", "") + "\n"
    str_to_sign += _get_canonicalized_xamz_headers(headers)

    # canonicalized_resource = _get_canonicalized_resource(canonical_uri, params)
    # replace canonical_uri with canonicalized_resource once
    # _get_canonicalized_resource() is implemented
    str_to_sign += canonical_uri

    str_to_sign = utf8_encode(str_to_sign)

    return str_to_sign

def sign_request_v2(method='GET', canonical_uri='/', params={}, headers={}):
    access_key = Credentials.access_key
    secret_key = Credentials.secret_key

    str_to_sign = _create_str_to_sign(method, canonical_uri, params, headers)

    signature = utf8_decode(base64.encodestring(
        hmac.new(utf8_encode(secret_key), str_to_sign, sha1).digest()).strip())

    auth_header = "AWS %s:%s" % (access_key, signature)

    return auth_header

def create_canonical_request(method, canonical_uri, body, epoch_t, host):
    canonical_query_string = "";
    signed_headers = 'host;x-amz-date'
    payload_hash = hashlib.sha256(body.encode('utf-8')).hexdigest()
    canonical_headers = 'host:' + host + '\n' + 'x-amz-date:' + get_timestamp(epoch_t) + '\n'
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
        algorithm='', host='' , service='', region=''):

    canonical_request = create_canonical_request(method, canonical_uri,
        body, epoch_t, host);

    credential_scope = get_date(epoch_t) + '/' + region + '/' + service + '/' + 'aws4_request'

    string_to_sign = algorithm + '\n' + get_timestamp(epoch_t) + '\n' + credential_scope \
    + '\n' +  hashlib.sha256(canonical_request.encode('utf-8')).hexdigest();
    return string_to_sign

def sign_request_v4(method=None, canonical_uri='/', body='', epoch_t='',
        host='', service='', region=''):

    if method is None:
        print("method can not be null")
        return None
    credential_scope = get_date(epoch_t) + '/' + region + \
        '/' + service + '/' + 'aws4_request'

    signed_headers = 'host;x-amz-date'

    algorithm = 'AWS4-HMAC-SHA256';

    access_key = Credentials.access_key
    secret_key = Credentials.secret_key

    string_to_sign = create_string_to_sign_v4(method, canonical_uri, body, epoch_t,
        algorithm, host, service, region)

    signing_key = getV4SignatureKey(secret_key, get_date(epoch_t), region, service);

    signature = hmac.new(signing_key, (string_to_sign).encode('utf-8'), hashlib.sha256).hexdigest();

    authorization_header = algorithm + ' ' + 'Credential=' + access_key + '/' + \
        credential_scope + ', ' +'SignedHeaders=' + signed_headers + \
        ', ' +  'Signature=' + signature
    return authorization_header

def get_date(epoch_t):
    return epoch_t.strftime('%Y%m%d')

def get_timestamp(epoch_t):
    return epoch_t.strftime('%Y%m%dT%H%M%SZ')
