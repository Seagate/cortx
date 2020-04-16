import json
from awss3api import S3PyCliTest

class AclTest(S3PyCliTest):

 def __init__(self, description):
     super(AclTest, self).__init__(description)

  # Validate complete acl with dummy acl object
 def validate_acl(self, response, owner_id, owner_name, grantee_id, grantee_name, grant_permission):

    expected_acl_string = """{"Owner": {"DisplayName": "%s","ID": "%s"},"Grants": [{"Grantee": {"Type": "CanonicalUser","DisplayName": "%s","ID": "%s"},"Permission": "%s"}]}"""\
    %(owner_name, owner_id, grantee_name, grantee_id, grant_permission)

    expected_acl_json = json.loads(expected_acl_string)
    actual_acl_json = json.loads(response.status.stdout)
    expected_acl = json.dumps(expected_acl_json, sort_keys=True)
    actual_acl = json.dumps(actual_acl_json, sort_keys=True)

    print("Expected acl  : [%s]" %(expected_acl))
    print("Response acl  : [%s]" %(actual_acl))
    assert expected_acl == actual_acl
    return self

  # Validate complete acl which includes two grants of type CanonicalUser
 def validate_acl_dual_grant(self, response, owner_id, owner_name, grantee_id, grantee_name, grant_permission, grantee_id2, grantee_name2, grant_permission2):

    expected_acl_string = """{"Owner": {"DisplayName": "%s","ID": "%s"},"Grants": [{"Grantee": {"Type": "CanonicalUser","DisplayName": "%s","ID": "%s"},"Permission": "%s"}, {"Grantee": {"Type": "CanonicalUser","DisplayName": "%s","ID": "%s"},"Permission": "%s"}]}"""\
    %(owner_name, owner_id, grantee_name, grantee_id, grant_permission, grantee_name2, grantee_id2, grant_permission2)

    expected_acl_json = json.loads(expected_acl_string)
    actual_acl_json = json.loads(response.status.stdout)
    expected_acl = json.dumps(expected_acl_json, sort_keys=True)
    actual_acl = json.dumps(actual_acl_json, sort_keys=True)

    print("Expected acl  : [%s]" %(expected_acl))
    print("Response acl  : [%s]" %(actual_acl))
    assert expected_acl == actual_acl
    return self

  # Validate acl which includes grants- one each for canonical user and group
 def validate_acl_single_group_grant(self, response, owner_id, owner_name, grantee_id, grantee_name, grant_permission, uri, group_permission):

    expected_acl_string = """{"Owner": {"DisplayName": "%s","ID": "%s"},"Grants": [{"Grantee": {"Type": "CanonicalUser","DisplayName": "%s","ID": "%s"},"Permission": "%s"}, {"Grantee": {"Type": "Group","URI": "%s"},"Permission": "%s"}]}"""\
    %(owner_name, owner_id, grantee_name, grantee_id, grant_permission, uri, group_permission)

    expected_acl_json = json.loads(expected_acl_string)
    actual_acl_json = json.loads(response.status.stdout)
    expected_acl = json.dumps(expected_acl_json, sort_keys=True)
    actual_acl = json.dumps(actual_acl_json, sort_keys=True)

    print("Expected acl  : [%s]" %(expected_acl))
    print("Response acl  : [%s]" %(actual_acl))
    assert expected_acl == actual_acl
    return self

  # Validate acl which includes grants- one each for canonical user and group
 def validate_acl_dual_group_grant(self, response, owner_id, owner_name, grantee_id, grantee_name, grant_permission, uri1, group_permission1, uri2, group_permission2):

    expected_acl_string = """{"Owner": {"DisplayName": "%s","ID": "%s"},"Grants": [{"Grantee": {"Type": "CanonicalUser","DisplayName": "%s","ID": "%s"},"Permission": "%s"}, {"Grantee": {"Type": "Group","URI": "%s"},"Permission": "%s"}, {"Grantee": {"Type": "Group","URI": "%s"},"Permission": "%s"}]}"""\
    %(owner_name, owner_id, grantee_name, grantee_id, grant_permission, uri1, group_permission1, uri2, group_permission2)

    expected_acl_json = json.loads(expected_acl_string)
    actual_acl_json = json.loads(response.status.stdout)
    expected_acl = json.dumps(expected_acl_json, sort_keys=True)
    actual_acl = json.dumps(actual_acl_json, sort_keys=True)

    print("Expected acl  : [%s]" %(expected_acl))
    print("Response acl  : [%s]" %(actual_acl))
    assert expected_acl == actual_acl
    return self

  #Check command returns valid response code
 def check_response_status(self, response):
     code = response.status.returncode
     if code != 0:
        raise AssertionError("Command failed with invalid response code")
     return self

  #Check the user details are valid or not
 def validate_details(self, user, user_id, user_name):
     assert "ID" in user
     assert "DisplayName" in user
     id = user["ID"]
     name = user["DisplayName"]
     assert isinstance(id, str)
     assert isinstance(name, str)
     assert id == user_id
     assert name == user_name
     return self

  #Check grant has valid permission
 def validate_permission(self, permission, value):
     acl_permission = ["READ", "WRITE", "FULL_CONTROL", "READ_ACP", "WRITE_ACP"]
     assert permission in acl_permission
     assert isinstance(permission, str)
     assert permission == value
     return self

  #Check default acl has valid Owner
 def validate_owner(self, response, owner_id, owner_name):
     acl_json = json.loads(response.status.stdout)
     assert "Owner" in acl_json
     owner = acl_json["Owner"]
     self.validate_details(owner, owner_id, owner_name)
     return self

  #Check default acl has valid Grant element
 def validate_grant(self, response, grantee_id, grantee_name, number_of_grants, grant_permission):

     acl_json = json.loads(response.status.stdout)
     assert "Grants" in acl_json
     grants = acl_json["Grants"]
     assert len(grants) == number_of_grants

     # Validate Grant
     for grant in grants:
         assert "Grantee" in grant
         assert "Permission" in grant

         #Validate Grantee
         grantee = grant["Grantee"]
         assert "Type" in grantee
         user_type = grantee["Type"]

         if user_type == "CanonicalUser":
            self.validate_details(grantee, grantee_id, grantee_name)
         elif user_type == "Group":
            assert "URI" in grantee
            assert isinstance(grantee["URI"], str)
         elif user_type == "Group":
            assert "Email" in grantee
            assert isinstance(grantee["Email"], str)

         # Validate Permission
         permission = grant["Permission"]
         self.validate_permission(permission, grant_permission)

     return self

