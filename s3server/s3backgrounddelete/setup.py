import os
from setuptools import setup
files = ["config/*", "VERSION"]

# Load the version
s3backgrounddelete_version = "1.0.0"
current_script_path = os.path.abspath(os.path.dirname(__file__))
with open(os.path.join(current_script_path, 'VERSION')) as version_file:
    s3backgrounddelete_version = version_file.read().strip()

setup(
    # Application name:
    name="s3backgrounddelete",

    # Version number (initial):
    version=s3backgrounddelete_version,

    # Application author details:
    author="Seagate",

    # Packages
    packages=["s3backgrounddelete"],

    # Include additional files into the package
    include_package_data=True,

    # Details
    scripts =['s3backgrounddelete/s3backgroundproducer', 's3backgrounddelete/s3backgroundconsumer'],

    # license="LICENSE.txt",
    description="Background delete process to clean stale oids in s3",

    package_data = { 's3backgrounddelete': files},

    # Dependent packages (distributions)
    install_requires=[
    'httplib2'
  ]
)
