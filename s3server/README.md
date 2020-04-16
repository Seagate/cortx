# For now dev centric readme
All the steps below are automated using ansible.
Please see <source>/ansible/readme

# Updation to nginx.conf.sample -- Added client_max_body_size to have max body size as 5GB

## How to install clang-format
Install clang-format for code formatting.
```sh
cd ~/Downloads/
wget http://llvm.org/releases/3.8.0/clang+llvm-3.8.0-linux-x86_64-centos6.tar.xz
tar -xvJf clang+llvm-3.8.0-linux-x86_64-centos6.tar.xz
sudo ln -s ~/Downloads/clang+llvm-3.8.0-linux-x86_64-centos6/bin/clang-format /bin/clang-format
```

## How to install git-clang-format
Install git-clang-format to run clang-format only on new changes.
```sh
cd ~/Downloads/
wget https://raw.githubusercontent.com/llvm-mirror/clang/master/tools/clang-format/git-clang-format
chmod +x git-clang-format
sudo cp git-clang-format /usr/bin
git config --global clangFormat.style 'Google'
```

## How to use git clang-format
Once you are ready with your changes to be committed, use below sequence of commands:
```sh
git clang-format --diff <files>   //prints the changes clang-format would make in next command
git clang-format -f <files>       //makes formatting changes in specified files
git add <files>
git commit
```

## How to inform clang-format to ignore code for formatting
If you don't want clang-format to work on a section of code then surround it
with `// clang-format off` and `// clang-format on`
```cpp

#include <iostream>
int main() {
// clang-format off
  std::cout << "Hello world!";
  return 0;
// clang-format on
}

```

## How to Build & Install S3 server, Auth server, UTs & third party libs?
Build steps for Dev environment and for release environment differ slightly.
In case of Dev, we locally build the mero source and use the mero libs from
the source code location. Whereas in case of Release, we assume that mero rpms
are pre-installed and use mero libs from standard location.

Steps for Dev environment:
```sh
./refresh_thirdparty.sh
./rebuildall.sh --no-mero-rpm
```

The `./refresh_thirdparty.sh` command refreshes the third party source code.
It will undo any changes made in third party submodules source code and will
clone missing submodules. Normally after a fresh repo clone, this command
needs to be executed only once.

The `./rebuildall.sh --no-mero-rpm` command will build third party libs, S3
server, Auth server, UTs etc. It will also install S3 server, Auth server &
third party libs at `/opt/seagate` location. To skip installing S3 use
--no-install.

Note the option `--no-mero-rpm` passed to the command. It informs the script that
mero source was built and mero libs from the source code location would be used.
If this option is absent, mero libs are used from mero rpm installed on system.
To skip installing S3 use --no-install.

`--use-build-cache` is useful when building with third_party, mero used from
local builds. Normally third party libs needs to be built only once after fresh
repo clone. If builds are not already cached, it will be built.
Note that this option is ignored in rpm based builds.

To skip rebuilding third party libs on subsequent runs of
`./rebuildall.sh`, use `--use-build-cache` option.
`--use-build-cache` option indicates that previously built third_party libs
are present in $HOME/.seagate_src_cache and will be used in current build.
```sh
./rebuildall.sh --no-mero-rpm --use-build-cache
```

## Build SSL certificates and install (use defaults)

# For single certificate with multidomain dns please refer,
# multidomain_ssl_certificate.md file from <s3 src>/docs folder

# By default we are using this multidomain certificate for s3 server and authserver

# For dev vm setup, check whether following line exists
`s3_ip_address=127.0.0.1` in <s3 src>/scripts/ssl/domain_input.conf

# For release/rpmbuild setup, update S3server V4 ip-address appropriately in
`s3_ip_address=x.x.x.x` in <s3 src>/scripts/ssl/domain_input.conf

# For custom domains, update endpoints appropriately
For example customer domains like `s3.some.customer.domain.com` update following
entries in <s3 src>/scripts/ssl/domain_input.conf

`s3_default_endpoint=s3.some.customer.domain.com`
`s3_region_endpoint=s3-us-west-2.some.customer.domain.com`
`s3_iam_endpoint=iam.some.customer.domain.com`
`s3_sts_endpoint=sts.some.customer.domain.com`

```sh
cd rpms/s3certs
# Here "s3dev" is a tag used on the generated certificate rpms
# Alternatively one can also pass other options, check -h.
./buildrpm.sh -T s3dev
yum localinstall ~/rpmbuild/RPMS/x86_64/stx-s3-certs*
yum localinstall ~/rpmbuild/RPMS/x86_64/stx-s3-client-certs*
```

Enable OpenLDAP SSL port and deploy SSL certificates, S3 Auth Server uses
SSL port of OpenLDAP for connection, below script needs to be run before
starting S3 Auth Server.
Ensure you have stx-s3-certs package installed before running this.
```sh
cd scripts/ldap/ssl
./enable_ssl_openldap.sh -cafile /etc/ssl/stx-s3/openldap/ca.crt \
      -certfile /etc/ssl/stx-s3/openldap/s3openldap.crt \
      -keyfile /etc/ssl/stx-s3/openldap/s3openldap.key
```
```sh
cp <s3-src>/scripts/ldap/ssl/ldap.conf /etc/openldap/ldap.conf
```

If current third_party/* revision does not match with previous revision
cached in $HOME/.seagate_src_cache/, user should clean the cache and rebuild.

Steps for Release environment:
Make sure mero rpms are installed on the build machine before executing
below commands.
```sh
./refresh_thirdparty.sh
./rebuildall.sh
```

## How to run auth server (this current assumes all dependencies are on same local VM)
```sh
sudo systemctl start s3authserver
```

## How to stop auth server (this current assumes all dependencies are on same local VM)
```sh
sudo systemctl stop s3authserver
```

## Running S3 Authserver listening on IPv4 or IPv6 only or both

Enable IPv6 only
   Set defaultHost value to ::1 or IPv6 address of local machine in /opt/seagate/auth/resources/authserver.properties

   Restart Authserver

Enable IPv4 only
   Set defaultHost value to 127.0.0.1 or IPv4 address of local machine in /opt/seagate/auth/resources/authserver.properties

   Restart Authserver

Enable both IPv6 and IPv4 on dual stack machine
   Set defaultHost value to 0.0.0.0 in /opt/seagate/auth/resources/authserver.properties

   Restart Authserver

## Load balancer for S3
We use haproxy as a load balancer, as well as have support for nginx. We started with
nginx and moved to haproxy during Hermi release.
Haproxy listens on port 80(http)/443(https) and forwards traffic to running S3 instances.

In case of CentOS 7:
```sh
yum install haproxy
cp <s3-src>/scripts/haproxy/haproxy_osver7.cfg /etc/haproxy/haproxy.cfg
cp <s3-src>/scripts/haproxy/503.http /etc/haproxy/503.http
```

In case of RHEL/CentOS 8:
```sh
yum install haproxy
cp <s3-src>/scripts/haproxy/haproxy_osver8.cfg /etc/haproxy/haproxy.cfg
cp <s3-src>/scripts/haproxy/503.http /etc/haproxy/503.http
```

## Auth Server health check
haproxy makes HEAD request with uri "/auth/health" to Auth server with interval
of 2 seconds and Auth server returns `200 OK` response for this request.
If you want to change frequency of health check, HAProxy config needs to be updated.
for example:
 default-server inter 2s fastinter 100 rise 1 fall 5 on-error fastinter

For more info, please refer below link
https://cbonte.github.io/haproxy-dconv/1.7/configuration.html#5.2-inter

## Install SSL certificates for S3 service (haproxy)
See Build SSL section above.
```sh
yum localinstall stx-s3-certs
systemctl restart haproxy
```

## How to have SSL enabled for S3 service (end to end ssl communication)
In /etc/haproxy/haproxy.cfg replace line
server s3-instance-1 0.0.0.0:8081 check
with
server s3-instance-1 0.0.0.0:8081 check ssl verify required ca-file /etc/ssl/stx-s3/s3/ca.crt
In /opt/seagate/s3/conf/s3config.yaml file have option S3_SERVER_SSL_ENABLE
set to true


## How to start/stop single instance of S3 server in Dev environment for testing?
Execute below command from `s3server` top level directory. Before executing below
commands, make sure that S3 Server, Auth server, third party libs etc are  built
& installed using `./rebuildall.sh --no-mero-rpm` command. Also make sure S3 Auth
server and Mero services are up & running.
```sh
sudo ./dev-starts3.sh
```
If more than one instance of S3 server needs to be started then pass number of
instances to start as an argument to above command. e.g. to start 4 instances:
```sh
sudo ./dev-starts3.sh 4
```

To stop S3 server in Dev environment, use below command
```sh
sudo ./dev-stops3.sh
```
Above command will stop all instances of S3 server.

## How to run Auth server tests?
```sh
cd auth
mvn test
cd -
```

## How to run S3 server Unit tests in Dev environment?
```sh
./runalltest.sh --no-mero-rpm --no-st-run
```
Above command runs S3 server UTs. Note the option `--no-mero-rpm` passed
to the command. It informs the script to use mero libs from the source code
location at the run time. In case of Release environment, simply skip passing
the option to the script.

## How to run S3 server standard (ST + UT) tests?
Python virtual env needs to be setup to run the STs. This is a one time step
after fresh repo clone.
```sh
cd st/clitests
./setup.sh
cd ../../
```
The `./setup.sh` script will display few entries to be added to the `/etc/hosts`
file. Go ahead and add those entries.
If you are not running as root then add user to /etc/sudoers file as below:
```sh
sudo visudo

# Find the line `root ALL=(ALL) ALL`, add following line after current line:
<user_name> ALL=(ALL) NOPASSWD:ALL

# Warning: This gives super user privilege to all commands when invoked as sudo.

# Now add s3server binary path to sudo secure path
# Find line with variable `secure_path`, append below to the varible
:/opt/seagate/s3/bin
```

Now setup to run STs is complete. Other details of ST setup can be found at
`st/clitests/readme`.
Use below command to run (ST + UT) tests in Dev environment.
```sh
./runalltest.sh --no-mero-rpm
```
In case of Release environment, simply skip passing the option `--no-mero-rpm` to
the script.

## How to run S3 server ossperf tests in Dev environment?
```sh
./runalltest.sh --no-mero-rpm --no-st-run --no-ut-run
```
Above command runs S3 server ossperf tool tests(Parallel/Sequential workloads).
Note the option `--no-mero-rpm` passed to the command. It informs the script
to use mero libs from the source code location at the run time.
In case of Release environment, simply skip passing the option to the script.

## How to run systemtest over HTTP during jenkins (Default uses HTTPS in jenkins).
```sh
vim scripts/haproxy/haproxy.cfg
```
Uncomment following line

server s3authserver 0.0.0.0:9085

```sh
vim s3config.yaml
```
Update following parameters value

S3_ENABLE_AUTH_SSL: false
S3_AUTH_PORT: 9085

```sh
vim auth/resources/authserver.properties
```
Update following parameters value

enableSSLToLdap=false
enable_https=false
enableHttpsToS3=false

Update jenkins script to use http

```sh
vim jenkins-build.sh
```
Specify `--no-https` to below script so that jenkins would excecute ST's over HTTP.

./runalltest.sh --no-mero-rpm --no-https


## How to run tests for unsupported S3 APIs?
For unsupported S3 APIs, we return 501(NotImplemented) error code.
To run system tests for unsupported S3 APIs, S3 server needs to be run in authentication
disabled mode. This can be done by running 'sudo ./dev-starts3-noauth.sh'.
This script starts S3Server in bypass mode by specifying disable_auth=true.
Then from st/clitests folder you need to run 'st/clitests/unsupported_api_check.sh'.
This script verifies if for all unimplemented S3 APIs we are returning correct error code.

## How to run s3 server via systemctl on deployment m/c
```sh
sudo systemctl start "s3server@0x7200000000000001:0x30"
```
where `0x7200000000000001:0x30` is the process FID assigned by halon

## How to stop s3 server service via systemctl on deployment m/c
```sh
sudo systemctl stop "s3server@0x7200000000000001:0x30"
```

## How to see status of s3 service on deployment m/c
```sh
sudo systemctl status "s3server@0x7200000000000001:0x30"
```

## Steps to create Java key store and Certificate.
## Required JDK 1.8.0_91 to be installed to run "keytool"
```sh
keytool -genkeypair -keyalg RSA -alias s3auth -keystore s3authserver.jks -storepass seagate -keypass seagate -validity 3600 -keysize 2048 -dname "C=IN, ST=Maharashtra, L=Pune, O=Seagate, OU=S3, CN=iam.seagate.com" -ext SAN=dns:iam.seagate.com,dns:sts.seagate.com,dns:s3.seagate.com
```

## Steps to generate crt from Key store
```sh
keytool -importkeystore -srckeystore s3authserver.jks -destkeystore s3authserver.p12 -srcstoretype jks -deststoretype pkcs12 -srcstorepass seagate -deststorepass seagate
```

```sh
openssl pkcs12 -in s3authserver.p12 -out s3authserver.jks.pem -passin pass:seagate -passout pass:seagate
```

```sh
openssl x509 -in s3authserver.jks.pem -out s3authserver.crt
```

#Steps to create pem file containing private and public key to deploy with haproxy for auth server endpoint.

#Extract Private Key

```sh
openssl pkcs12 -in s3authserver.p12 -nocerts -out s3authserver.jks.key -passin pass:seagate -passout pass:seagate
```
#Decrypt using pass phrase

```sh
openssl rsa -in s3authserver.jks.key -out s3authserver.key -passin pass:seagate
cat s3authserver.crt s3authserver.key > s3authserver.pem
```

## Steps to create Key Pair for password encryption and store it in java keystore
**This key pair will be used by AuthPassEncryptCLI and AuthServer for encryption and decryption of ldap password respectively**
```sh
keytool -genkeypair -keyalg RSA -alias s3auth_pass -keystore s3authserver.jks -storepass seagate -keypass seagate -validity 3600 -keysize 512 -dname "C=IN, ST=Maharashtra, L=Pune, O=Seagate, OU=S3, CN=iam.seagate.com" -ext SAN=dns:iam.seagate.com,dns:sts.seagate.com,dns:s3.seagate.com
```

## How to generate S3 server RPM
Following dependencies are required to build rpms.
```sh
yum install ruby
yum install ruby-devel
gem install bundler
gem install fpm
```
To generate s3server RPMs, follow build & install steps from above section
`Steps for Release environment`. After that execute below command:
```sh
./makerpm
# Above should generate a rpm file in current folder
```
### How to get unit test code coverage for Authserver?
```sh
$ cd auth
$ ./mvnbuild.sh clean

# Generate code coverage report
$ ./mvnbuild.sh package

# Code coverage report is generated as part of mvn build
# coverage report location are:
#     a) executable : s3server/auth/server/target/coverage-reports/jacoco.exec
#     b) html       : s3server/auth/server/target/site/jacoco/index.html
#     c) xml        : s3server/auth/server/target/site/jacoco/jacoco.xml
#     d) csv        : s3server/auth/server/target/site/jacoco/jacoco.csv
# surefire-reports: s3server/auth/server/target/surefire-reports
# java classes : s3server/auth/server/target/classes

```
### How to get system test code coverage for Authserver?
```sh
cd auth
$ ./mvnbuild.sh clean
$ ./mvnbuild.sh package

# Start Mero server
$ cd s3server/third_party/mero
$ ./m0t1fs/../clovis/st/utils/mero_services.sh start

# Run authserver with jacoco agent
$ java -javaagent:/path/to/jacocoagent.jar=destfile=target/coverage-reports/jacoco.exec,append=false \
  -jar /path/to/AuthServer-1.0-0.jar

# Example:
# Note: This example uses the jacocoagent jar file downloaded by maven in local repo
# Maven local repo path: ${HOME}/.m2/repository
# S3 server location : ${s3repository} e.g. /home/720368/s3repository/s3server
$ java \
-javaagent:${HOME}/.m2/repository/org/jacoco/org.jacoco.agent/0.8.4/\
org.jacoco.agent-0.8.4-runtime.jar=destfile=target/coverage-reports/\
jacoco.exec,append=false -jar ${s3repository}/auth/target/AuthServer-1.0-0.jar

# Start s3 server
$ cd s3server
$ ./dev-starts3.sh

# Run system test
$ cd s3server/st/clitest
$ python3 auth_spec.py

# Stop auth server [ Ctrl + c ].

# Stop Mero, s3 server
$ cd s3server/third_party/mero
$ ./m0t1fs/../clovis/st/utils/mero_services.sh stop
$ cd s3server/
$ ./dev-stops3.sh

# Generate coverage report site from coverage data file generated in above step
$ ./mvnbuild.sh jacoco-report

# Code coverage report is generated at
#     a) executable : s3server/auth/server/target/coverage-reports/jacoco.exec
#     b) html       : s3server/auth/server/target/site/jacoco/index.html
#     c) xml        : s3server/auth/server/target/site/jacoco/jacoco.xml
#     d) csv        : s3server/auth/server/target/site/jacoco/jacoco.csv
# surefire-reports: s3server/auth/server/target/surefire-reports
# java classes : s3server/auth/server/target/classes


```
### How to setup ssl
```sh
$ cd ssl

# This script will generate ssl certificates and display information on ssl setup.
$ ./setup.sh

```

## StatsD configuration
Install statsd using
```sh
sudo yum install statsd
```

By default, Stats feature is disabled. To enable the same, edit the S3 server
config file /opt/seagate/s3/conf/s3config.yaml & set the S3_ENABLE_STATS to true.
After above config change, s3server needs to be restarted.

Before starting StatsD daemon, select backends to be used. StatsD can send data
to multiple backends. By default, through config file, StatsD is configured to
send data only to console backend. To enable sending data to Graphite backend,
after s3server installation, edit the file /etc/statsd/config.js
Refer to s3statsd-config.js file in our source repo and copy if required.
Un-comment lines having Graphite variables (graphiteHost & graphitePort) and set their
values. Also add graphite to the backends variable as shown in the comment in the
s3statsd-config.js file.

##  Adding S3 Node Identifier to StatsD
By default statsd sends metrics with prefix "stats", if multiple statsd send data to
graphite, all of them will be listed in same prefix name. To differentiate metrics
from different S3 nodes statsd config file needs to be updated with globalPrefix
value, for example below globalePrefix value is set to "s3node2".
```
{
    graphitePort: 2003,
    graphiteHost: "graphite-host",
    mgmt_port: 8126,
    port: 8125,
    graphite: {
        legacyNamespace: false,
        globalPrefix: "s3node2"
    }
    , backends: [
        "./backends/graphite" ,
        "./backends/console"
    ]
}
```

Once above config is done, run StatsD daemon as below
```sh
sudo systemctl restart statsd
```

## Sending sample test data to StatsD

```sh
echo "myapp.myservice:10|c" | nc -w 1 -u localhost 8125
```

## Viewing StatsD data
a) Console backend
StatsD data can be viewed from /var/log/messages file. Alternatively the data
can also be viewed by telnetting to the management port 8126. The port number
is configurable in the s3statsd-config.js file. Common commands are:
help, stats, counters, timers, gauges, delcounters, deltimers, delgauges,
health, config, quit etc.

eg:
```sh
$ echo "help" | nc 127.0.0.1 8126
  Commands: stats, counters, timers, gauges, delcounters, deltimers, delgauges,
            health, config, quit

$ echo "stats" | nc 127.0.0.1 8126
  uptime: 30
  messages.last_msg_seen: 30
  messages.bad_lines_seen: 0
  console.lastFlush: 1481173145
  console.lastException: 1481173145
  END

$ echo "counters" | nc 127.0.0.1 8126
  { 'statsd.bad_lines_seen': 0,
    'statsd.packets_received': 1,
    'statsd.metrics_received': 1,
    total_request_count: 1 }
  END

```

b) Graphite backend
Open a browser on the machine hosting Graphite. Type 127.0.0.1 as the URL.
Graphite will show a dashboard, select a metric name. Graphite will display
the corresponding stats data in the form of graphs.

## Graphite installation
For the sake of simplicity, install Graphite on a VM running Ubuntu 14.04 LTS.

```sh
# Download synthesize which in turn will install Graphite
cd ~/Downloads/
wget https://github.com/obfuscurity/synthesize/archive/v2.4.0.tar.gz
tar -xvzf v2.4.0.tar.gz
cd synthesize-2.4.0/

# Edit install file: comment out lines concerning installation of collectd and
statsite.

# Run the install script
sudo ./install

# Refer to https://github.com/etsy/statsd/blob/master/docs/graphite.md and
  edit files:
    /etc/carbon/storage-schemas.conf and
    /etc/carbon/storage-aggregation.conf

# Restart Graphite
sudo service carbon-cache restart

#Installing Graphite on CentOS
yum install -y graphite-web python-carbon

#Set the retention period in storage schema
vim /etc/carbon/storage-schemas.conf

#Add entry at the end
#  [name]
#  pattern = regex
#  retentions = timePerPoint:timeToStore, timePerPoint:timeToStore, ...

[default]
pattern = .*
retentions = 12s:4h, 2m:3d, 5m:8d, 13m:32d, 1h:1y

#Restart Carbon service
sudo systemctl enable carbon-cache
sudo systemctl restart carbon-cache

#Run database setup script
#change the timezone and SECRET_KEY values in /etc/graphite-web/local_settings.py(Optional)

PYTHONPATH=/usr/share/graphite/webapp django-admin syncdb --settings=graphite.settings

#Creating a superuser
#You just installed Django's auth system, which means you don't have any superusers defined.
Would you like to create one now? (yes/no): Please enter either "yes" or "no": yes
Username (leave blank to use 'root'): <username>
Email address: <email>
Password: <password>
Password (again): <password>

#Configure Apache for Graphite

#Remove default index page from Apache
echo > /etc/httpd/conf.d/welcome.conf

#Edit /etc/httpd/conf.d/graphite-web.conf and replace everything in the 'Directory "/usr/share/graphite/"' block with
Require all granted
Order allow,deny
Allow from all

#Assign permission to Graphite dir
sudo chown apache:apache /var/lib/graphite-web/graphite.db

#Work around a bug related to building index with
touch /var/lib/graphite-web/index

#Start Apache and enable auto-start
sudo systemctl start httpd
sudo systemctl enable httpd

#Access to Graphite Web interface
#Enable port 80 in firewalld
sudo firewall-cmd --permanent --add-service=http
sudo firewall-cmd --reload

#If Graphite is running on different node enable port 2003
firewall-cmd --zone=public --add-port=2003/tcp --permanent
sudo firewall-cmd --reload

```

## Bareos Setup

### Follow [Ansible](https://www.ansible.com/) steps to configure [Bareos](https://www.bareos.org/en/) on VM.
```sh
cd <s3-src>/ansible
cat readme

```

* Update director configuration file

```sh
vi /etc/bareos/bareos-dir.d/storage/s3_storage.conf

```

>**Update "Address" field with fully qualified domain name(FQDN)
>The "Password" field should match "Password" of /etc/bareos/bareos-sd.d/director/bareos-dir.conf**


* Ensure port 80/443 in s3server/Mero Node is open

```sh
iptables -I INPUT -p tcp -m tcp --dport 80 -j ACCEPT

```
* Update file selection in fileSet resource definition

```sh
vi /etc/bareos/bareos-dir.d/fileset/s3files.conf

```

* All files in "File" are selected for backup/restore. Ensure they are present locally on VM.

* Update jobdefs resource file

```sh
vi  /etc/bareos/bareos-dir.d/jobdefs/S3Job.conf

```

* Update "Level" to select Full/Incremental/Diﬀerential backup.

* Create/Update job resource definition file for backup/restore.Sample files are:

```sh
vim /etc/bareos/bareos-dir.d/job/BackupToS3.conf
vim /etc/bareos/bareos-dir.d/job/RestoreFiles.conf

```

* Start bareos daemons/services

```sh
systemctl start bareos-dir
systemctl start bareos-sd
systemctl start bareos-fd

```

* Storage Daemon Node should be able to resolve s3server/mero host <Bucket Name>.s3.seagate.com

* Append following entries to /etc/hosts

```sh

192.168.64.144 seagatebucket.s3.seagate.com
192.168.64.144 iam.seagate.com sts.seagate.com s3.seagate.com

```

* Running a backup job.

```sh
[root@localhost bareos]# bconsole
*run
The defined Job resources are:
     1: RestoreFiles
     2: BackupToS3
     3: BackupCatalog
     4: backup-bareos-fd
Select Job resource (1-4): 2

```

* Running a restore job.


```sh
*restore
Select you job id.
cwd is: /
$ mark *
$ done

```

### Verifying files that have been restore

* The restored files will be present in /tmp/bareos-restores

* Use "md5sum" command to verify hash of files that have been restored.

```sh
md5sum /tmp/bareos-restores/file.txt
md5sum /root/file.txt

```

# Audit logging

Audit logging supports 3 types of loggers:
* rsyslog via tcp
* rsyslog via syslog call
* log4cxx.
* also it could be disabled.

Following settings are responsible for audit logging
* S3_AUDIT_LOG_CONFIG - path to configuration file for log4cxx logger
* S3_AUDIT_LOG_FORMAT_TYPE - format of the audit log records; possible values are "JSON" and "S3_FORMAT"
* S3_AUDIT_LOGGER_POLICY - type of the logger, possible values are
     - disabled - logger disabled
     - rsyslog-tcp - log to rsyslog via tcp
     - syslog - log to rsyslog via syslog call
     - log4cxx - log with log4cxx
* S3_AUDIT_LOGGER_HOST - rsyslog host name to connect to
* S3_AUDIT_LOGGER_PORT - rsyslog port to connect to
* S3_AUDIT_LOGGER_RSYSLOG_MSGID - rsyslog msgid to filter messages

## log4cxx

* S3_AUDIT_LOG_CONFIG: "/opt/seagate/s3/conf/s3server_audit_log.properties"
* S3_AUDIT_LOGGER_POLICY: "log4cxx"

## syslog

* S3_AUDIT_LOGGER_POLICY: "syslog"
* S3_AUDIT_LOGGER_RSYSLOG_MSGID: "s3server-audit-logging"

## rsyslog-tcp

* S3_AUDIT_LOGGER_POLICY: "rsyslog-tcp"
* S3_AUDIT_LOGGER_HOST: localhost
* S3_AUDIT_LOGGER_PORT: 514
* S3_AUDIT_LOGGER_RSYSLOG_MSGID: "s3server-audit-logging"

## disabled

* S3_AUDIT_LOGGER_POLICY: "disabled"

## other values will lead to s3server stop

## Notes

* in case of **rsyslog-tcp** and **syslog** s3server config values must correspond to
rsyslog settings from /etc/rsyslog.d/ directory

* default configuration file for rsyslog is s3server/scripts/rsyslog-tcp-audit.conf

* configuration file for rsyslog should be copied manually to /etc/rsyslog.d/ dir

* rsyslog should be started before s3server

* any changes to s3server config and/or to rsyslog config should be done in the following order
     - stop s3server
     - stop rsyslog
     - change configs
     - start rsyslog
     - start s3server


# Clovis Stubs

There are number of command line parameters to fake clovis functionality.
It is possible to use all of them at once or just a subset
* --fake_clovis_writeobj - stub for clovis write object with all zeros
* --fake_clovis_readobj - stub for clovis read object with all zeros
* --fake_clovis_createidx - stub for clovis create idx - does nothing
* --fake_clovis_deleteidx - stub for clovis delete idx - does nothing
* --fake_clovis_getkv - stub for clovis get key-value - read from memory hash map
* --fake_clovis_putkv - stub for clovis put kye-value - stores in memory hash map
* --fake_clovis_deletekv - stub for clovis delete key-value - deletes from memory hash map

Note: for proper KV mocking one should use following combination
```
--fake_clovis_createidx true --fake_clovis_deleteidx true --fake_clovis_getkv true --fake_clovis_putkv true --fake_clovis_deletekv true
```

# Call graph

Call graph contains relationship between functions, number of calls of each
funtion and number of instructions executed. These info is extarcted with help
of valgrind tool
```
valgrind --tool=callgrind <s3server cmd>
```

Note: valgrind profiling is extreamly slow.

**dev-starts3.sh** is extended with **--callgraph** option. The option has a
parameter - the path to valgrind raw data.

Note: if **--callgraph** option is mentioned only one instance of s3server will be run.

There two ways to analyze valgrind raw data. The first one is to use GUI
application called KCachegrind. This is the prefered way since KCachegrind has
lots drawing and analitics features.
The other way is to convert it to text form. This is done automatically by
providing the same option to **dev-stops3.sh** script.

**dev-stops3.sh** is extended with **--callgraph** option. The option has a
parameter - the path to valgrind raw data. It should be the same name provided
to **dev-starts3.sh --callgraph <path-to-file>**. After the s3server stopped and
valgrind raw data generated, tool called **callgrind_annotate** converts it
to text form. Converted file will have the same name as raw file plus
"*.annotated" suffix.

**jenkins-build.sh** is extended with **--callgraph** option. The option has a
parameter - the path to valgrind raw data. If valgrind package were not found in
the system it will be installed with yum package manager.

Note: if **jenkins-build.sh** were run with **--skip_tests** option s3server will
not be stopped and **dev-stops3.sh** should be called with the same **--callgraph**
option value.

Note: **--callgraph** option could be used together with **--basic_test_only**, e.g.
```
jenkins-build.sh --skip_build --basic_test_only --callgraph /tmp/callgraph.out
```
or
```
jenkins-build.sh --skip_build --fake_obj --fake_kvs --basic_test_only --callgraph /tmp/callgraph.out
```
in both cases s3server instance will be run, simple **s3cmd** commands for
file put/get are executed, s3server will be stopped.
There will be two output files created
/tmp/callgraph.out - raw valgrind generated file and
/tmp/callgraph.out.annotated - text form

# Deployment from rpm

The **rpm-deploy.sh** is used to deploy s3server, mero and halon on test node.

Note: **dev/init.sh** script should be run before rpm deployment

Note: **rpm-deploy.sh** should not be used outside source tree

Deployment on a clean node:

1 - Run **dev/init.sh** or make sure it was already run

2 - Update yum repos to install mero and halon from; By default hermi repos are
used - **http://ci-storage.mero.colo.seagate.com/releases/hermi/last_successful/mero/repo**;
if one needs to use binaries from specific sprint following command should be run
```
./rpm-deploy.sh -y ees1.0.0-PI.1-sprint4
```
where **ees1.0.0-PI.1-sprint4** is a sprint name, after that repos named
**http://ci-storage.mero.colo.seagate.com/releases/eos/ees1.0.0-PI.1-sprint4/mero/repo**,
**http://ci-storage.mero.colo.seagate.com/releases/eos/ees1.0.0-PI.1-sprint4/halon/repo**,
**http://ci-storage.mero.colo.seagate.com/releases/eos/ees1.0.0-PI.1-sprint4/s3server/repo**
will be added and prioritized against hermi repos;
To be able to restore default repos one should run
```
./rpm-deploy.sh -y hermi
```

3 - Remove all existing packages if any
```
./rpm-deploy.sh -R
```
this cmd will try to uninstall following pkgs: s3server, s3server-debuginfo,
mero, mero-devel, halon, s3iamcli

4 - Install packages
```
./rpm-deploy.sh -I
```
mero, mero-devel, halon and s3iamcli packages will be installed from the yum repo;
s3server package will be built from the current source tree

5 - Run status command and make sure all packages installed and configured
```
./rpm-deploy.sh -S
```

6 - If s3server config is not a production config run
```
./rpm-deploy.sh -p <@ or path-to-desired-config-file>
```
if one needs to use **s3config.release.yaml** config from source tree the "@"
should be used

7 - Start cluster
```
./rpm-deploy.sh -U
```

8 - Stop cluster
```
./rpm-deploy.sh -D
```

Commands 2-7 could be combined
```
./rpm-deploy.sh -y ees1.0.0-PI.1-sprint4 -RI -p @ -US
```
this command will update repos, remove pkgs, install pkgs, update s3server config,
run cluster and print statuses of all installed pkgs

### Enable debug logs for openldap
# by default slapd log level is set 0
Log level '0' will generate only slapd service start log messages
Use other log levels mentioned in openldap documentation,
https://www.openldap.org/doc/admin24/slapdconfig.html see "6.2.1.5. loglevel <level>" section

# How to change log level
1. Update <s3 src>/scripts/ldap/slapdlog.ldif file and change the log level
as mentioned in the openldap documentation

2. modify ldap by running below script
```
ldapmodify -Y EXTERNAL -H ldapi:/// -w ldapadmin -f <s3 src>/scripts/ldap/slapdlog.ldif
```
3. verify log level is been set properly
```
ldapsearch -w seagate -x -D cn=admin,cn=config -b cn=config | grep olcLogLevel
```
4. restart slapd service
```
systemctl restart slapd
```
5. check slapd log file
```
vim /var/log/slapdlog.log
```
