#!/bin/bash

#install pylint
echo "installing pylint...."
yum -y install pylint > /dev/null

#check if pylint is installed or not
if [ "rpm -q pylint" ]
then
   version=$(rpm -q --qf "%{Version}" pylint)
   echo "pylint $version is installed successfully"
else
  echo "pylint installation failed"
fi

echo "installing autopep8...."
#install autopep8 :automatically formats python code
#to conform to the PEP 8 style guide.
pip3 install --upgrade autopep8 2> /dev/null

#check if autopep8 is installed or not
if [ "pip3 list | grep autopep8" ]
then
   autopep8version=$(pip3 show autopep8 | grep Version)
   echo "autopep8 $autopep8version is installed successfully"
else
  echo "autopep8 installation failed"
fi

