## Tools :
1. Pylint   : Checks for errors,warnings in python code and generate reports.
2. Autopep8 : Automatically formats Python code to conform to the PEP 8 style.

----
## Setup
# Run setup.sh to install Pylint and Autopep8

    ./setup.sh

----
## How to run pylint and autopep8
# This will generate report at <OUTPUT_DIR>/pylint-report/report.txt

    ./python_check.sh <SOURCE_DIR> <OUTPUT_DIR> [--autofix | --dryrunfix]

> Note: we are using pylint configuration file (i.e pylint_config.file) in order to follow google python style guide.
> For more info about google python style guide, please see : http://google.github.io/styleguide/pyguide.html
> and for configuration file, please see : https://github.com/google/yapf/blob/master/pylintrc

----
## Test examples
** only reports :

    ./python_check.sh /var/seagate/s3server/s3backgrounddelete /var/log/seagate/pyreports

> Note: Only python lint report will be generated at OUTPUT_DIR/pylint-report/report.txt
> Developer has to fix all warnings,errors.

** with --dryrunfix :

    ./python_check.sh /var/seagate/s3server/s3backgrounddelete /var/log/seagate/pyreports --dryrunfix

> Note: this will list out all files that autopep8 will autofix in order to conform to PEP 8 style guide.
> This will also generate detailed dryrunfix report at OUTPUT_DIR/dryrunfix_report.txt.
> if Developer want to use autofix then they should run command agaian with --autofix
> if not then run commmand without any options.

** with --autofix :

     ./python_check.sh /var/seagate/s3server/s3backgrounddelete /var/log/seagate/pyreports --autofix

> Note: this will format python code in SOURCE_DIR to conform to PEP 8 style guide.
> It will not fix all the warnings or errors.
> Please check list of fixes :[autopep8](https://github.com/hhatto/autopep8)
> Developer needs to check pylint report to fix remaining warnigs and errors.

