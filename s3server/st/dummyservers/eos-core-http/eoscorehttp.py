from flask import Flask
from flask import request
from flask import Response
from flask import abort

import json

app = Flask(__name__)

# Dummy server for testing purpose.
# Globals:
#       in memory KeyValue store
#       in memory object store
# Index sample {
#                 "idx_1" : {"K1": "V1", "K2": "V2"},
#                 "idx_2" : {"K1": "V1", "K2": "V2"}
#               }
_index_store = {}

# Object sample {
#                 "obj_1" : "Dummy object data",
#                 "obj_2" : "Dummy object data",
#               }
_object_store = {}


@app.route('/help')
def help():
    return '''
    <u>Supported APIs.</u>
</br>
    <u>Index APIs:</u></br>
      GET /indexes/idx1</br>
      PUT /indexes/idx1</br>
</br>
    <u>Key Value APIs:</u></br>
      GET /indexes/idx1/key1</br>
      PUT /indexes/idx1/key1 {Value in body}</br>
      DELETE /indexes/idx1/key1</br>
</br>
    <u>Object APIs:</u></br>
      GET /objects/obj1</br>
      PUT /objects/obj1</br>
      DELETE /objects/obj1</br>
</br>
    '''


# Index APIs
def error_if_index_absent(index_id):
    if index_id not in _index_store:
        abort(Response("Index not found.", 404))

@app.route('/indexes/<index_id>', methods = ['GET', 'PUT'])
def process_index_api(index_id):
    if request.method == 'GET':
        error_if_index_absent(index_id)
        return json.dumps(_index_store[index_id])
    elif request.method == 'PUT':
        if index_id in _index_store:
            return ("Index already exists.", 409)
        else:
            _index_store[index_id] = {}
        return ("Created Index.", 201)

@app.route('/indexes/<index_id>/<key>', methods = ['GET', 'PUT', 'DELETE'])
def process_key_val_api(index_id, key):
    error_if_index_absent(index_id)

    if request.method == 'GET':
        if key in _index_store[index_id]:
            return _index_store[index_id][key]
        else:
            return ("Key not found.", 404)
    elif request.method == 'PUT':
        value = request.get_data()
        _index_store[index_id][key] = value.decode("utf-8")
        return ("Created Key.", 201)
    elif request.method == 'DELETE':
        if key in _index_store[index_id]:
            del _index_store[index_id][key]
        return ("Deleted Key.", 204)
    else:
        return "Method not supported"

# Object APIs
@app.route('/objects/<object_id>', methods = ['GET', 'PUT', 'DELETE'])
def process_object_api(object_id):
    if request.method == 'GET':
        if object_id in _object_store:
            return _object_store[object_id]
        else:
            abort(Response("Object not found.", 404))
    elif request.method == 'PUT':
        if object_id in _object_store:
            abort(Response("Object already exists.", 409))
        else:
            value = request.get_data()
            if value is None:
                value = "Dummy object data."
            _object_store[object_id] = value.decode("utf-8")
        return ("Created Object.", 201)
    elif request.method == 'DELETE':
        if object_id in _object_store:
            del _object_store[object_id]
        return ("Deleted Object.", 204)
    else:
        return "Method not supported"
