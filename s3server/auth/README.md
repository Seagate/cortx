## Auth Server health check
Provides health check mechanism for auth server.
When Auth server receives request with uri '/auth/health', it sends http
response with status 200 OK
If this response is 200 OK, that means Auth server is healty and running

curl command : curl -s -I -k -X HEAD https://iam.seagate.com:9443/auth/health

