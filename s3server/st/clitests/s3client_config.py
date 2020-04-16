class S3ClientConfig:
    access_key_id = ""
    secret_key = ""
    token = ""
    ldapuser = ""
    ldappasswd = ""
    pathstyle = False
    ca_file = "/etc/ssl/stx-s3-clients/s3/ca.crt"
    auth_ca_file = "/etc/ssl/stx-s3-clients/s3/ca.crt"
    s3_uri_https = "https://s3.seagate.com"
    iam_uri_https = "https://iam.seagate.com:9443"
    s3_uri_http = "http://s3.seagate.com"
    iam_uri_http = "http://iam.seagate.com:9085"
