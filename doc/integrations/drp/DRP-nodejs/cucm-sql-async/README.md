# cucm-sql
Cisco Unified Communications Manager SQL


      var util = require("util");
      var cucmsql = require('cucm-sql');
      var cucm = cucmsql("cucmpublisher.example.com", 'username', "password");
      
      var DIRECTORY_NUMBER_PATTERN="5060%";
      
      SQL = "select name, numplan.description, dnorpattern from device, numplan, devicenumplanmap where device.pkid=devicenumplanmap.fkdevice and numplan.pkid=devicenumplanmap.fknumplan and name like 'SEP%' and dnorpattern like '%s'"
      
      SQL = util.format(SQL, DIRECTORY_NUMBER_PATTERN);
      
      
      cucm.query(SQL, function(err, response){
	      console.log(err, response);
      })
