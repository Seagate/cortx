use_https = false #default false, if set to true then ssl will be used
host = {{ s3_host_endpoint }}
access_key = {{ s3_access_key }}
secret_key = {{ s3_secret_key }}
pricing_dir = ""
backend = s3
aws_auth_sign_version = 4
