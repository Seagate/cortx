from setuptools import setup, find_namespace_packages
from pathlib import Path


here = Path(__file__).absolute().parent


def read_text(file_path):
    """ In Python 3.4 `Path` has no `read_text()`. """
    with open(str(file_path)) as f:
        return f.read()


long_description = read_text(here / 'README.md')
requirements = read_text(here / 'requirements.txt')
dev_requirements = read_text(here / 'dev-requirements.txt')

setup(
    name='cortx-siddhi',
    version='1',
    description='CORTX Siddhi Integration',
    long_description=long_description,
    long_description_content_type='text/markdown',
    author='Weka.IO',
    author_email='',
    classifiers=[
        'Intended Audience :: Developers',
        'License :: OSI Approved :: MIT License',
        'Programming Language :: Python :: 3.4',
    ],
    package_dir={'': 'src'},
    packages=find_namespace_packages(where='src'),
    python_requires='>=3.4, <4',
    install_requires=requirements,
    extras_require={
        'dev': dev_requirements,  # e.g.: "pip install 'cortx-siddhi[dev]'"
                                  #   or: "pip install '.[dev]'"
    },
    entry_points={
        'console_scripts': [
            'cortx-siddhi = cortx_siddhi.main:main',
        ],
    },
)
