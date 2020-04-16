import os
from setuptools import setup
files = ["config/*", "VERSION"]

# Load the version
s3iamcli_version = "1.0.0"
current_script_path = os.path.abspath(os.path.dirname(__file__))
with open(os.path.join(current_script_path, 'VERSION')) as version_file:
    s3iamcli_version = version_file.read().strip()

setup(
    # Application name:
    name="s3iamcli",

    # Version number (initial):
    version=s3iamcli_version,

    # Application author details:
    author="Seagate",

    # Packages
    packages=["s3iamcli"],

    # Include additional files into the package
    include_package_data=True,

    # Details
    scripts =['s3iamcli/s3iamcli'],

    # license="LICENSE.txt",
    description="Seagate S3 IAM CLI.",

    package_data = { 's3iamcli': files},

    # Dependent packages (distributions)
    install_requires=[
    'boto3',
    'botocore',
    'xmltodict',
    'pyyaml'
  ]
)
