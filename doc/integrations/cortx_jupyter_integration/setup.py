import setuptools

with open('README.md', 'r') as fh:
    long_description = fh.read()

setuptools.setup(
    name='cortx_jupyter',
    version='0.1.129',
    author='Sumanth & Priya',
    description='Jupyter Notebook Manager for Cortx',
    long_description=long_description,
    long_description_content_type= "text/markdown",
    url='https://github.com/sumanthreddym/cortx-jupyter',
    packages=setuptools.find_packages()
)
