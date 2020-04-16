package com.seagates3.acl;

import static org.junit.Assert.*;

import java.util.ArrayList;

import org.junit.Before;
import org.junit.Test;

import com.seagates3.authserver.AuthServerConfig;
import com.seagates3.exception.GrantListFullException;

public
class AccessControlListTest {

  AccessControlList acl = new AccessControlList();
  ArrayList<Grant> grantList;
  Grant defaultGrant;

  @Before public void setUp() throws Exception {
    grantList = new ArrayList<>();
    defaultGrant = new Grant(new Grantee("id1", "name1"), "FULL_CONTROL");
  }

  // Adds grant to grant list
  @Test public void testAddGrant_Success() throws GrantListFullException {
    acl.addGrant(defaultGrant);
    assertEquals(defaultGrant, acl.getGrantList().get(0));
    assertEquals(1, acl.getGrantList().size());
  }

  // Runs into GrantListFullException when adding more than 100 grants
  @Test(expected = GrantListFullException
                       .class) public void testAddGrant_GrantListFullException()
      throws GrantListFullException {
    for (int i = 0; i <= AuthServerConfig.MAX_GRANT_SIZE; i++)
      acl.addGrant(defaultGrant);
  }

  //  getGrant succeeds with matching canonical id
  @Test public void testGetGrant_Success() throws GrantListFullException {
    acl.addGrant(defaultGrant);
    assertEquals(defaultGrant, acl.getGrant("id1").get(0));
  }

  // getGrant returns null for invalid canonical id
  @Test public void testGetGrant_NotFound() throws GrantListFullException {
    acl.addGrant(defaultGrant);
    assertEquals(0, acl.getGrant("id2").size());
  }

  // getGrant returns null for null canonical id
  @Test public void testGetGrant_NullID() throws GrantListFullException {
    acl.addGrant(defaultGrant);
    assertEquals(null, acl.getGrant(null));
  }
}
