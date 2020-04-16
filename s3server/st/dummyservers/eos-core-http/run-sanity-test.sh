#!/bin/sh -x

curl http://localhost:5000/indexes/idx1 ; echo

curl -X PUT http://localhost:5000/indexes/idx1 ; echo

curl http://localhost:5000/indexes/idx1 ; echo

curl http://localhost:5000/indexes/idx1/key1 ; echo

curl -X PUT -d Value1 http://localhost:5000/indexes/idx1/key1 ; echo

curl http://localhost:5000/indexes/idx1/key1 ; echo

curl http://localhost:5000/indexes/idx1 ; echo

curl -X DELETE http://localhost:5000/indexes/idx1/key1 ; echo

curl http://localhost:5000/indexes/idx1 ; echo

curl http://localhost:5000/indexes/idx1/key1 ; echo

curl http://localhost:5000/objects/obj1 ; echo

curl -X PUT http://localhost:5000/objects/obj1 ; echo

curl -X DELETE http://localhost:5000/objects/obj1 ; echo

curl http://localhost:5000/objects/obj1 ; echo
