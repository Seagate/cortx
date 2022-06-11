set DOMAINNAME=mydomain.xyz
set MESHKEY=supersecretkey
set PORT=8082
set ZONENAME=zone1
set DEBUG=true
set TESTMODE=true
set AUTHENTICATORSERVICE=
start "Registry1" node drpRegistry.js
timeout /T 3
set PORT=8083
start "Registry2" node drpRegistry.js
timeout /T 1
set PORT=8084
set ZONENAME=zone2
start "Registry3" node drpRegistry.js
timeout /T 1
set PORT=8085
start "Registry4" node drpRegistry.js
timeout /T 1
set PORT=
set ZONENAME=zone1
start "Authenticator" node drpProvider-Test-Authenticator.js
timeout /T 1
set PORT=8080
start "Broker1" node drpBroker.js
timeout /T 1
set PORT=8081
set ZONENAME=zone2
start "Broker2" node drpBroker.js
timeout /T 1
set PORT=
set SCOPE=zone
set ZONENAME=zone1
start "TestService1-zone1" node drpProvider-Test-NoListener.js
timeout /T 1
start "TestService2-zone1" node drpProvider-Test-NoListener.js
timeout /T 1
set ZONENAME=zone2
start "TestService1-zone2" node drpProvider-Test-NoListener.js
timeout /T 1
start "TestService2-zone2" node drpProvider-Test-NoListener.js
timeout /T 1
set PORT=
set SCOPE=global
set ZONENAME=zone1
start "TestService-Hive" node drpProvider-Hive.js
start "TestService-DocMgr" node drpProvider-DocMgr.js