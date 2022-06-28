from distutils.core import setup

setup(name='cortx_pytorch',
      version='1.0',
      description='Fast interface between pytorch and Segate CORTX',
      author='Guillaume Leclerc',
      author_email='leclerc@mit.edu',
      url='https://github.com/GuillaumeLeclerc/cortx_pytorch',
      packages=['cortx_pytorch'],
      install_requires=[
          'torch',
          'tqdm',
          'boto3',
          'webdataset'
      ]
     )
