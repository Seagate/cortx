#!/bin/bash

#Script to check Python coding style and formatting issues.
#Note: Only Python files are checked for coding style.

if [ $# -le 1 ] || [ $# -ge 4 ]
then
  printf "%s\n%s\n\t%s\n\t%s\n\t%s\n" \
         "Invalid number of arguments to the script..." \
         "Enter the directories to scan and to generate pylint report." \
         "Usage : ./python_check.sh <SOURCE_DIR> <OUTPUT_DIR> [--autofix | --dryrunfix]" \
         "Optional : --dryrunfix : will show which files will be modified if --autofix flag is set." \
         "Optional : --autofix   : will fix the warnings,errors as per PEP 8 style guide."
  exit 1
fi

#Get directory location to save pylint output.
SOURCE_DIR=$1
OUTPUT_DIR=$2
REPORT_LOCATION=$OUTPUT_DIR/pylint-report

rm -rf $REPORT_LOCATION 2> /dev/null

mkdir -p $REPORT_LOCATION 2> /dev/null

SCRIPT_PATH=$(readlink -f "$0")
CURRENT_DIR=$(dirname "$SCRIPT_PATH")

#####################Autopep8###############
#Check if --autofix or --dryrunfix flag is set
if [ -z $3 ]
then
     printf "%s\n" "Flag :--dryrunfix or --autofix is not set so ignoring autofix..."
else
    if [ $3 == "--dryrunfix" ]
    then
        printf "%s\n" "--dryrunfix used: Lists out all files which will be autofixed if --autofix is selected..."

        AUTOPEP8_OPTIONS="--recursive --diff"
        autopep8 $AUTOPEP8_OPTIONS $SOURCE_DIR | tee $OUTPUT_DIR/dryrunfix_report.txt | grep  -Po '\+++ fixed/\K[^\n]+'

        printf "%s\n%s\n" "dry run for autofix is completed..." \
                          "Please see detailed dryrunfix report at $OUTPUT_DIR/dryrunfix_report.txt" \
                          "if you are ok with these modifications then run this command again with --autofix option" \
                          "else run this command with no option to generate lint report"
        exit 0

    elif [ $3 == "--autofix" ]
    then
        printf "%s\n" "--autofix used: *WARNING*: Updating files to auto fix possible pep8 deviations..."

        AUTOPEP8_OPTIONS="--in-place --recursive --aggressive"
        autopep8 $AUTOPEP8_OPTIONS $SOURCE_DIR

        printf "%s\n" "autofix is completed and modified files are:"
        git status -uall -s $SOURCE_DIR | grep  -E '\.py$'
    else
        printf "%s\n" "Invalid parameter : $3 found " \
               "Usage : ./python_check.sh <SOURCE_DIR> <OUTPUT_DIR> [--autofix | --dryrunfix]"
        exit 1
    fi
fi

###### generate pylint report #######
#Add pylint options
printf "%s\n" "generating pylint report.."
OPTIONS="--rcfile=$CURRENT_DIR/pylint_config.file"

cd $REPORT_LOCATION
find $SOURCE_DIR -type f -name "*.py" | xargs pylint $OPTIONS > $REPORT_LOCATION/report.txt


#Check if report file is generated or not
if [ -e $REPORT_LOCATION/report.txt ]
then
    printf "%b\n" "python lint report is generated at $REPORT_LOCATION"
else
    printf "%b\n" "python lint report is not generated.please check pylint configuration"
fi
