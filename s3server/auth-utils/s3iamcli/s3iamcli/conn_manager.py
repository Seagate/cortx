import socket
import json
import xmltodict
import ssl
import http.client
import pprint
import sys
import os
import urllib

from s3iamcli.config import Config

class ConnMan:

    def _get_ssl_verified_context():
        cert_file = Config.ca_cert_file
        if cert_file is None or not os.path.isfile(cert_file):
            print("ERROR: Please specify certificate file.", file=sys.stderr)
            sys.exit(1)
        context = ssl.create_default_context(cafile=cert_file)
        if not Config.check_ssl_hostname:
            context.check_hostname = False
        context.verify_mode = ssl.CERT_REQUIRED
        return context

    def _get_ssl_unverified_context():
        context = ssl.create_default_context()
        context.check_hostname = False
        context.verify_mode = ssl.CERT_NONE
        return context

    def _get_https_connection():
        if Config.verify_ssl_cert:
            context = ConnMan._get_ssl_verified_context()
        else:
            context = ConnMan._get_ssl_unverified_context()
        endpoint_url = urllib.parse.urlparse(Config.endpoint).netloc
        conn = http.client.HTTPSConnection(endpoint_url, context = context)
        return conn

    def _get_http_connection():
        endpoint_url = urllib.parse.urlparse(Config.endpoint).netloc
        return http.client.HTTPConnection(endpoint_url)

    def _get_connection():
        if Config.use_ssl:
            return ConnMan._get_https_connection()
        else:
            return ConnMan._get_http_connection()

    def send_post_request(body, headers=None):
        conn = ConnMan._get_connection()
        if headers == None:
            headers = {"Content-type": "application/x-www-form-urlencoded", "Accept": "text/plain"}
        conn.request('POST', '/', body, headers)
        response = conn.getresponse()
        result = {'status': response.status, 'headers': response.getheaders(),
                'body': response.read(), 'reason': response.reason}
        conn.close()
        return result
