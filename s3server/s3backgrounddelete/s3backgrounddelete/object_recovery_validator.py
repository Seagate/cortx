"""
ObjectRecoveryValidator acts as object validator which performs necessary action for object oid to be deleted.
"""
import logging
import json
from datetime import datetime

from s3backgrounddelete.eos_core_kv_api import EOSCoreKVApi
from s3backgrounddelete.eos_core_object_api import EOSCoreObjectApi
from s3backgrounddelete.eos_core_index_api import EOSCoreIndexApi
import math

#zero/null object oid in base64 encoded format
NULL_OBJ_OID = "AAAAAAAAAAA=-AAAAAAAAAAA="

class ObjectRecoveryValidator:
    """This class is implementation of Validator for object recovery."""

    def __init__(self, config, probable_delete_records,
                 logger=None, objectapi=None, kvapi=None, indexapi=None):
        """Initialise Validator"""
        self.config = config
        self.current_obj_in_VersionList = None
        self.probable_delete_records = probable_delete_records
        if(logger is None):
            self._logger = logging.getLogger("ObjectRecoveryValidator")
        else:
            self._logger = logger
        if(objectapi is None):
            self._objectapi = EOSCoreObjectApi(self.config, logger=self._logger)
        else:
            self._objectapi = objectapi
        if(kvapi is None):
            self._kvapi = EOSCoreKVApi(self.config, logger=self._logger)
        else:
            self._kvapi = kvapi
        if(indexapi is None):
            self._indexapi = EOSCoreIndexApi(self.config, logger=self._logger)
        else:
            self._indexapi = indexapi

    def isVersionEntryOlderThan(self, versionInfo, older_in_mins = 15):
        if (versionInfo is None):
            return False

        object_version_time = versionInfo["create_timestamp"]
        now = datetime.utcnow()
        date_time_obj = datetime.strptime(object_version_time, "%Y-%m-%dT%H:%M:%S.000Z")
        timedelta = now - date_time_obj
        timedelta_mns = math.floor(timedelta.total_seconds()/60)
        return (timedelta_mns >= older_in_mins)

    def delete_object_from_storage(self, obj_oid, layout_id):
        status = False
        ret, response = self._objectapi.delete(obj_oid, layout_id)
        if (ret):
            status = ret
            self._logger.info("Deleted obj " + obj_oid + " from mero store")
        elif (response.get_error_status() == 404):
            self._logger.info("The specified object " + obj_oid + " does not exist")
            status = True
        else:
            self._logger.info("Failed to delete obj " + obj_oid + " from mero store")
            self.logAPIResponse("VERSION DEL", "", obj_oid, response)
        return status

    def delete_index(self, index_id):
        ret, response = self._indexapi.delete(index_id)
        if (ret):
            self._logger.info("Deleted index: " + index_id)
        elif (response.get_error_status() == 404):
            # Index not found
            self._logger.info("Index " + index_id + " does not exist")
            ret = True
        else:
            self._logger.info("Failed to delete index " + index_id)
            self.logAPIResponse("DEL INDEX", index_id, "", response)
        return ret

    def delete_key_from_index(self, index_id, key_id, api_prefix):
        ret, response = self._kvapi.delete(index_id, key_id)
        if (ret):
            self._logger.info("Deleted Key " + key_id + " from index " + index_id)
        elif (response.get_error_status() == 404):
            # Item not found
            self._logger.info("key " + key_id + " does not exist in index " + index_id)
            ret = True
        else:
            self._logger.info("Failed to delete key " + key_id + " from index " + index_id)
            self.logAPIResponse(api_prefix, index_id, key_id, response)
        return ret

    def get_key_from_index(self, index_id, key):
        ret, response_data = self._kvapi.get(index_id, key)
        if (ret):
            # Found key in index
            self._logger.info("Key "+ str(key) + " exists in index " + str(index_id))
            return ret, response_data
        elif (response_data.get_error_status() == 404):
            self._logger.info("Key "+ str(key) + " does not exist in index " + str(index_id))
            return True, None
        else:
            self._logger.info("Failed to retrieve key " + str(key) + " from index " + str(index_id))
            self.logAPIResponse("GET INDEX", index_id, key, response_data)
            return ret, None

    def get_object_versionEntry(self, version_list_indx, version_key):
        versionInfo = None
        status = False
        ret, response_data = self.get_key_from_index(version_list_indx, version_key)
        if (ret):
            status = ret
            if (response_data is not None):
                # Found key in index
                self._logger.info("Version " + str(version_key) + " exists in version index " + str(version_list_indx))
                self._logger.info("Version details: " + str(response_data.get_value()))
                versionInfo = json.loads(response_data.get_value())
            else:
                self._logger.info("Version " + str(version_key) + " does not exist in version index " + str(version_list_indx))
        else:
            self._logger.info("Version " + str(version_key) + " does not exist in version index " + str(version_list_indx))

        return status, versionInfo

    def get_object_metadata(self, index_id, key):
        obj_metadata = None
        status, response = self.get_key_from_index(index_id, key)
        if (status and response is not None):
            obj_metadata = json.loads(response.get_value())
        return status, obj_metadata

    def logAPIResponse(self, resAPI, oid, key, response):
        if (response.get_error_status() != 200):
            self._logger.info("Failed API " + resAPI + " on Oid= " + str(oid) + ", Key= " + str(key) +
                              " Response: " + str(response.get_error_status()) + " " +
                              str(response.get_error_message()))

    def process_probable_delete_record(self, delete_entry = False, delete_obj_from_store = False):
        object_version_list_index_id = self.object_leak_info["objects_version_list_index_oid"]
        status = False
        obj_ver_key = None
        if ("true" != self.object_leak_info["is_multipart"]):
            obj_ver_key = self.object_leak_info["version_key_in_index"]

        if (delete_obj_from_store):
            # Delete mero object associated with version. Also, delete version entry
            if all(v is not None for v in [object_version_list_index_id, obj_ver_key]):
                status = self.del_obj_from_version_list(object_version_list_index_id, obj_ver_key)
                if (not status):
                    self._logger.info("Failed to delete version/object " + obj_ver_key +
                                      " from version list")
            else:
                return status

        leak_rec_key = self.probable_delete_records["Key"]
        # If 'delete_entry =True', then delete record from probable delete index
        if (delete_entry):
            probable_index_id = self.config.get_probable_delete_index_id()
            if (delete_entry and leak_rec_key is not None):
                status = self.delete_key_from_index(probable_index_id, leak_rec_key, "PROBABALE INDEX DEL")

        return status

    def del_obj_from_version_list(self, versionListIndx, versionKey):
        status = False
        if all(v is not None for v in [versionListIndx, versionKey]):
            # Fetch version from version list
            status, versionInfo = self.get_object_versionEntry(versionListIndx, versionKey)
            if (not status):
                self._logger.info("Error! Failed to get object with version key " + versionKey +
                    " from version list")
                return status

            if (versionInfo is not None):
                obj_oid = versionInfo["mero_oid"]
                layout_id = versionInfo["layout_id"]
                #Delete version object from mero store
                status = self.delete_object_from_storage(obj_oid, layout_id)
                if (status):
                    self._logger.info("Deleted object version with oid " + obj_oid + " from mero store")
                else:
                    self._logger.info("Failed to delete object version with oid [" + obj_oid + "] from mero store")
            else:
                self._logger.info("The version key: " + versionKey + " does not exist. Delete mero object")
                status = self.delete_object_from_storage(self.object_leak_id, self.object_leak_layout_id)

            if (status):
                status = self.delete_key_from_index(versionListIndx, versionKey, "VERSION LIST DEL")
                if (status):
                    self._logger.info("Deleted version key " + versionKey + " from version list index " + versionListIndx)

        return status

    def check_instance_is_nonactive(self, instance_id, marker=None):

        """Checks for existence of instance_id inside global instance index"""

        result, instance_response = self._indexapi.list(
                    self.config.get_global_instance_index_id(), self.config.get_max_keys(), marker)
        if(result):
            # List global instance index is success.
            self._logger.info("Index listing result :" +
                             str(instance_response.get_index_content()))
            global_instance_json = instance_response.get_index_content()
            global_instance_list = global_instance_json["Keys"]
            is_truncated = global_instance_json["IsTruncated"]

            if(global_instance_list is not None):
                for record in global_instance_list:
                    if(record["Value"] == instance_id):
                        # instance_id found. Skip entry and retry for delete oid again.
                        self._logger.info("S3 Instance is still active. Skipping delete operation")
                        return False

                if(is_truncated == "true"):
                    self.check_instance_is_nonactive(instance_id, global_instance_json["NextMarker"])

            # List global instance index results is empty hence instance_id not found.
            return True

        else:
            # List global instance index is failed.
            self._logger.error("Failed to list global instance index")
            return False

    def process_results(self):
        #Execute object leak algorithm by processing each of the entries from RabbitMQ
        probable_delete_oid = self.probable_delete_records["Key"]
        probable_delete_value = self.probable_delete_records["Value"]

        self._logger.info(
            "Probable object id to be deleted : " +
            probable_delete_oid)
        try:
            self.object_leak_info = json.loads(probable_delete_value)
            self.object_leak_id = probable_delete_oid
            self.object_leak_layout_id = self.object_leak_info["object_layout_id"]

        except ValueError as error:
            self._logger.error(
                "Failed to parse JSON data for: " + probable_delete_value + " due to: " + error)
            return

        # Assumption: Key = <current oid>-<new oid> for
        # an old object of PUT overwrite request
        if (self.object_leak_info["old_oid"] == NULL_OBJ_OID):
            # In general, this record is for old object
            # For old object of PUT overwrite request, the key of probable leak record contains 4 '-'
            #   Each object oid has 1 '-', seperating high and low values
            # e.g., variable 'probable_delete_oid' contains: "Tgj8AgAAAAA=-kwAAAAAABCY=-Tgj8AgAAAAA=-lgAAAAAABCY="
            # where old obj id is "Tgj8AgAAAAA=-kwAAAAAABCY=" and new obj id is "Tgj8AgAAAAA=-lgAAAAAABCY="
            oil_list = self.object_leak_id.split("-")
            if (oil_list is None or (len(oil_list) not in [2, 4])):
                self._logger.error("The key for old object " + str(self.object_leak_id) +
                                   " is not in the required format 'oldoid-newoid'")
                return
            self.object_leak_id = oil_list[0] + "-" + oil_list[1]

        # Determine object leak using information in metadata
        # Below is the implementaion of leak algorithm
        self.process_object_leak()

    def version_entry_cb(self, versionEntry, current_oid, timeVersionEntry):
        """ Processes each version entry. Return false to skip the entry. True to process it by caller"""
        if (versionEntry is None or current_oid is None):
            return False

        # Check if version entry is same as the current_oid
        if (versionEntry["mero_oid"] == current_oid):
            self.current_obj_in_VersionList = versionEntry
            return False

        # Check if version entry is recent (i.e. not older than timeVersionEntry)
        if (not self.isVersionEntryOlderThan(versionEntry, timeVersionEntry)):
            return False

        if (versionEntry["mero_oid"] != current_oid):
            return True


    def process_objects_in_versionlist(self, object_version_list_index, current_oid, callback, timeVersionEntry=15, marker = None):
        """
        Identify object leak due to parallel PUT using the version list.
        Initial marker should be: object key name + "/"
        """
        bStatus = False
        if (object_version_list_index is None or callback is None or current_oid is None):
            return bStatus
        self._logger.info("Processing version list for object leak oid " + self.object_leak_id)
        object_key = self.object_leak_info["object_key_in_index"]
        if (object_version_list_index is not None):
            extra_qparam = {'Prefix':object_key}
            ret, response_data = self._indexapi.list(object_version_list_index,
                self.config.get_max_keys(), marker, extra_qparam)
            if (ret):
                self._logger.info("Version listing result for object " + object_key + ": " +
                                str(response_data.get_index_content()))

                object_versions = response_data.get_index_content()
                object_version_list = object_versions["Keys"]
                is_truncated = object_versions["IsTruncated"]
                bStatus = ret
                if (object_version_list is not None):
                    self._logger.info("Processing " + str(len(object_version_list)) +
                        " objects in version list = " + str(object_version_list))

                    for object_version in object_version_list:
                        self._logger.info(
                            "Fetched object version: " +
                            str(object_version))
                        obj_ver_key = object_version["Key"]
                        obj_ver_md = json.loads(object_version["Value"])
                        # Call the callback to process version entry
                        cb_status = callback(obj_ver_md, current_oid, timeVersionEntry)

                        if (cb_status == True):
                            self._logger.info("Leak detected: Delete version object and version entry for key: " + obj_ver_key)
                            # Delete object from store and delete the version entry from the version list
                            status = self.del_obj_from_version_list(object_version_list_index, obj_ver_key)
                            if (status):
                                self._logger.info("Deleted leaked object at key: " + obj_ver_key)
                                # Delete entry from probbale delete list as well, if any
                                indx = self.config.get_probable_delete_index_id()
                                indx_key = obj_ver_md["mero_oid"]
                                self._logger.info("Deleting entry: " + indx_key + " from probbale list")
                                status = self.delete_key_from_index(indx, indx_key, "PROBABLE INDEX DEL")
                                if (status):
                                    self._logger.info("Deleted entry: " + indx_key + " from probbale list")
                            else:
                                self._logger.info("Error! Failed to delete leaked object at key: " + obj_ver_key)
                                return False
                else:
                    self._logger.info("Error: Failed to list object versions")
                    return False

                last_key = object_versions["NextMarker"]
                if (is_truncated and last_key.startswith(object_key)):
                    bStatus = self.process_objects_in_versionlist(object_version_list_index,
                        obj_ver_key, callback, timeVersionEntry, last_key)

                return bStatus

            if (ret is False):
                self._logger.info("Failed to get Object version listing for object: " + self.object_key +
                    " Error: " + str(response_data))
                if (response_data.get_error_status() == 404):
                    self._logger.info("Object " + object_key + " is Not found(404) in the version list")

        return bStatus

    def process_object_leak(self):
        self._logger.info("Processing object leak for oid: " + self.object_leak_id)

        # Object leak detection algo: Step #2
        # Check if 'forceDelete' is set on leak entry. If yes, delete object and record from probable leak table
        force_delete = self.object_leak_info["force_delete"]
        if (force_delete == "true"):
            if ("true" != self.object_leak_info["is_multipart"]):
                # This is not a multipart request
                status = self.process_probable_delete_record(True, True)
                if (status):
                    self._logger.info("Leak entry " + self.object_leak_id + " processed successfully and deleted")
                else:
                    self._logger.error("Failed to process leak oid " + self.object_leak_id)
            else:
                # This is a multipart request(Post complete OR Multipart Abort).
                # Delete only object(no versions, as version does not exist yet) and then
                # delete entry from probable delete index.
                oid = self.object_leak_id
                self._logger.info("Object " + self.object_leak_id + " is for multipart request")
                layout = self.object_leak_info["object_layout_id"]
                status = self.delete_object_from_storage(oid, layout)
                if (status):
                    self._logger.info("Object for Leak entry " + self.object_leak_id + " deleted from store")
                    status = self.process_probable_delete_record(True, False)
                    if (status):
                        self._logger.info("Leak entry " + self.object_leak_id + " processed successfully and deleted")
                    else:
                        self._logger.error("Failed to process leak oid " + self.object_leak_id + " Failed to delete entry from leak index")
                else:
                    self._logger.error("Failed to process leak oid, failed to delete object  " +
                        self.object_leak_id + " Skipping entry for next run")

                # Delete part list index, if any
                part_list_index_id = self.object_leak_info["part_list_idx_oid"]
                if (part_list_index_id):
                    status = self.delete_index(part_list_index_id)
                    if (status):
                        self._logger.info("Deleted part list index " + str(part_list_index_id) + " successfully")
                    else:
                        self._logger.info("Failed to delete part list index " + str(part_list_index_id))
            return

        obj_key = self.object_leak_info["object_key_in_index"]
        obj_list_id = self.object_leak_info["object_list_index_oid"]

        if (force_delete == "false"):
            # For multipart new object request, check if entry exists in multipart metadata
            # If entry does not exist, delete the object and associated leak entry from probabale delete list
            if ("true" == self.object_leak_info["is_multipart"]):
                multipart_indx = obj_list_id
                self._logger.info("Object " + self.object_leak_id + " is for multipart request with force_delete=False")
                #Check object exists in multipart metadata index
                status, response = self.get_key_from_index(multipart_indx, obj_key)
                if (status):
                    if (response is None):
                        self._logger.info("Leak entry " + self.object_leak_id + " does not exist in multipart index")
                        # This is a multipart request(Post complete OR Multipart Abort).
                        # Delete only object(no versions, as version does not exist yet) and then
                        # delete entry from probable delete index.
                        oid = self.object_leak_id
                        layout = self.object_leak_info["object_layout_id"]
                        status = self.delete_object_from_storage(oid, layout)
                        if (status):
                            self._logger.info("Object for Leak entry " + self.object_leak_id + " deleted from store")
                            status = self.process_probable_delete_record(True, False)
                            if (status):
                                self._logger.info("Leak entry " + self.object_leak_id + " processed successfully and deleted")
                            else:
                                self._logger.error("Failed to process leak oid " + self.object_leak_id +
                                    " Failed to delete entry from leak index")
                        else:
                            self._logger.error("Failed to process leak oid, failed to delete object " +
                                self.object_leak_id + " Skipping entry for next run")

                        # Delete part list index, if any exists
                        part_list_index_id = self.object_leak_info["part_list_idx_oid"]
                        if (part_list_index_id):
                            status = self.delete_index(part_list_index_id)
                            if (status):
                                self._logger.info("Deleted part list index " +  str(part_list_index_id) + " successfully")
                            else:
                                self._logger.info("Failed to delete part list index " + str(part_list_index_id))
                    else:
                        self._logger.info("Skipping leak entry " + self.object_leak_id + " as it exists in multipart index")
                else:
                    self._logger.error("Failed to process leak oid " + self.object_leak_id +
                        "Skipping entry. Failed to search multipart index")
                return

        status, current_object_md = self.get_object_metadata(obj_list_id, obj_key)

        if (status):
            # Either object exists or object does not exist.
            if (current_object_md is None):
                # Object does not exist.
                self._logger.info("Object key " + obj_key + " does not exist in object list index. " +
                    "Delete mero oid and the leak entry")
                status = self.process_probable_delete_record(True, True)
                if (status):
                    self._logger.info("Leak oid " + self.object_leak_id + " processed successfully and deleted")
                else:
                    self._logger.error("Failed to process leak oid " + self.object_leak_id)
                return
            else:
                # Object exists. Contine further with leak algorithm.
                pass
        else:
            self._logger.error("Failed to process leak oid " + self.object_leak_id +
                        " Skipping entry for next run")
            return

        # Object leak detection algo: Step #3 - For old object
        current_oid = current_object_md["mero_oid"]
        # If leak entry is corresponding to old object
        if (self.object_leak_info["old_oid"] == NULL_OBJ_OID):
            # If old object is different than current object in metadata
            if (self.object_leak_id != current_oid):
                # This means old object is no more current/live, delete it
                self._logger.info("Leak oid: " + self.object_leak_id + " does not match the current. "
                    + "Attempting to delete it")
                status = self.process_probable_delete_record(True, True)
                if (status):
                    self._logger.info("Leak oid {Old} " + self.object_leak_id + " processed successfully and deleted")
                else:
                    self._logger.info("Error!Failed to delete Leak oid {Old} " + self.object_leak_id)
                return
            else:
                # old object is still current/live as per metadata
                # Check if there was any server crash
                instance_id = self.object_leak_info["global_instance_id"]
                if(self.check_instance_is_nonactive(instance_id)):
                    self._logger.info("Old object leak oid " + str(self.object_leak_id) +
                                      " is not asscociated with active S3 instance. Deleting it...")
                    status = self.process_probable_delete_record(True, True)
                    if (status):
                        self._logger.info("Leak oid " + self.object_leak_id + " processed successfully and deleted")
                    else:
                        self._logger.error("Failed to discard leak oid " + self.object_leak_id)
                else:
                    # Ignore and process leak record in next cycle
                    self._logger.info("Skip processing old object leak oid " + str(self.object_leak_id))
                return

        # Object leak detection algo: Step #4 - For new object
        if (self.object_leak_info["old_oid"] != NULL_OBJ_OID):
            # If new object is same as the current object in metadata
            if (self.object_leak_id == current_oid):
                # One of the previous PUT operations is successfull. Check if this has
                # introduced any leak due to parallel PUT operations
                timeDelayVersionProcessing = self.config.get_version_processing_delay_in_mins()
                object_version_list_index_id = self.object_leak_info["objects_version_list_index_oid"]
                obj_ver_key = self.object_leak_info["version_key_in_index"]
                self._logger.info("Processing version list for new object oid " + self.object_leak_id)
                self.process_objects_in_versionlist(object_version_list_index_id, current_oid,
                    self.version_entry_cb, timeDelayVersionProcessing, obj_key + "/")
                # After processing leak due to parallel PUT, delete probable record as new object is current/live object
                status = self.process_probable_delete_record(True, False)
                if (status):
                    self._logger.info("New object oid " + self.object_leak_id +
                        " is discarded from probable delete list")
                else:
                    self._logger.info("Failed to process new object oid " + self.object_leak_id)
            else:
                # Check if the request is in progress.
                # For this, check if new object oid is present in version table
                ovli_oid = self.object_leak_info["objects_version_list_index_oid"]
                ovli_key = self.object_leak_info["version_key_in_index"]

                status, versioninfo = self.get_object_versionEntry(ovli_oid, ovli_key)

                if (status and versioninfo is not None):
                    self._logger.info("New obj oid: " + self.object_leak_id + " with version key:" +
                        ovli_key + " exists in version table")
                    # new object is in the version table
                    # This indicates object write was complete. Check if version metadata update is in-progress
                    # Is object in version list older than 5mins
                    bCheck = self.isVersionEntryOlderThan(versioninfo, 5)
                    if (bCheck):
                        # Version table entry was done, but latest metadata update might have failed.
                        # From S3 perspective, this object was never live.
                        # Delete new-oid object, delete probable record
                        self.process_probable_delete_record(True, True)
                    else:
                        # version metadata update is likely in progress, give it some time, ignore the
                        # record and process it in next cycle
                        self._logger.info("Skipping processing of new obj oid: " + self.object_leak_id +
                            " to a later time")
                        pass
                elif (status and versioninfo is None):
                    self._logger.info("New obj oid: " + self.object_leak_id + " with version key:" +
                        ovli_key + " does not exist in version table. Check S3 instance exist")
                    # new object is not in the version table
                    # Check if LC of the record has changed, indicating server crash
                    instance_id = self.object_leak_info["global_instance_id"]
                    if (self.check_instance_is_nonactive(instance_id)):
                        # S3 process working on new-oid has died without updating metadata.
                        # new-oid can be safely deleted, delete probable record.
                        status = self.process_probable_delete_record(True, True)
                        if (status):
                            self._logger.info("New obj oid " + self.object_leak_id + " is deleted and discarded")
                    else:
                        # S3 process is working on new-oid. Ignore the record to be processed
                        # in next schedule cycle
                        self._logger.info("Skipping processing of new obj oid: " + self.object_leak_id)
                        pass
                else:
                    self._logger.info("Failed to process new obj oid: " + self.object_leak_id +
                        "Skipping to next cycle...")
                    pass
        return
