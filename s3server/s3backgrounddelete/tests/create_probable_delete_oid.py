"""This will generate sample data and test s3 background delete functionality."""
#!/usr/bin/python3
import sys
import os

sys.path.append(
    os.path.abspath(os.path.join(os.path.dirname(__file__), os.path.pardir)))

from eos_core_config import EOSCoreConfig
from eos_core_kv_api import EOSCoreKVApi
from eos_core_index_api import EOSCoreIndexApi
from eos_core_object_api import EOSCoreObjectApi


# Create sample data for s3 background delete.
if __name__ == "__main__":
    CONFIG = EOSCoreConfig()
    EOSCoreIndexApi(CONFIG).put("probable_delete_index_id")
    EOSCoreKVApi(CONFIG).put(
        "probable_delete_index_id",
        "oid-1",
        "{ \"obj-name\" : \"bucket_1/obj_1\"}")
    EOSCoreKVApi(CONFIG).put(
        "probable_delete_index_id",
        "oid-2",
        "{ \"obj-name\" : \"bucket_1/obj_2\"}")
    EOSCoreIndexApi(CONFIG).list("probable_delete_index_id")
    EOSCoreKVApi(CONFIG).get("probable_delete_index_id", "oid-1")
    EOSCoreKVApi(CONFIG).get("probable_delete_index_id", "oid-3")

    # Sample object metadata
    '''{
        "ACL" : "",
        "Bucket-Name" : "seagatebucket",
        "Object-Name" : "SC16Builds_v1.tar.gz",
        "Object-URI" : "seagatebucket\\\\SC16Builds_v1.tar.gz",
        "System-Defined": {
            "Content-Length" : "69435580",
            "Content-MD5" : "30baeecadf7417f842f5b9088b73f8a1-5",
            "Date" : "2016-10-24T09:01:03.000Z",
            "Last-Modified" : "2016-10-24T09:01:03.000Z",
            "Owner-Account" : "s3_test",
            "Owner-Account-id" : "12345",
            "Owner-User" : "tester",
            "Owner-User-id" : "123",
            "x-amz-server-side-encryption" : "None",
            "x-amz-server-side-encryption-aws-kms-key-id" : "",
            "x-amz-server-side-encryption-customer-algorithm" : "",
            "x-amz-server-side-encryption-customer-key" : "",
            "x-amz-server-side-encryption-customer-key-MD5" : "",
            "x-amz-storage-class" : "STANDARD",
            "x-amz-version-id" : "",
            "x-amz-website-redirect-location" : "None"
        },
        "User-Defined": {
            "x-amz-meta-s3cmd": "true",
            "mero_oid_u_hi" : "CzhfWNjoNAA=",
            "mero_oid_u_lo" : "FzVUkUG8V+0=",
            }
    '''

    OBJECTID1_METADATA = "{\"Object-Name\":\"bucket_1/obj_1\", \
                           \"x-amz-meta-s3cmd\": \"true\", \
                           \"mero_oid_u_hi\" : \"oid-1\", \
                           \"mero_oid_u_lo\" : \"FzVUkUG8V+0=\"}"

    OBJECTID2_METADATA = "{\"Object-Name\":\"bucket_1/obj_2\", \
                           \"x-amz-meta-s3cmd\": \"true\", \
                           \"mero_oid_u_hi\" : \"oid-2\", \
                           \"mero_oid_u_lo\" : \"FzVUkUG8V+0=\"}"

    EOSCoreIndexApi(CONFIG).put("object_metadata_index_id")
    EOSCoreKVApi(CONFIG).put(
        "object_metadata_index_id",
        "oid-1",
        OBJECTID1_METADATA)
    EOSCoreKVApi(CONFIG).put(
        "object_metadata_index_id",
        "oid-2",
        OBJECTID2_METADATA)
    EOSCoreIndexApi(CONFIG).list("object_metadata_index_id")
    EOSCoreKVApi(CONFIG).get("object_metadata_index_id", "oid-1")
    EOSCoreKVApi(CONFIG).get("object_metadata_index_id", "oid-2")
    EOSCoreObjectApi(CONFIG).delete("0x-7200000000000000x0","test")
