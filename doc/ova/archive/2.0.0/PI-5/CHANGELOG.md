This release notes includes new features, enhancements, and bug fixes added to the CORTX k8s OVA.


## Bug Fixes

- init and data containers are not ordered across the pods[EOS-26888]
- Support hctl status over http command[EOS-26482]
- Modify consul endpoint handling to connect to endpoint with tcp protocol[EOS-26729]
- hctl status takes too much time in containers[EOS-25703]
- Reduce mkfs time for multiple io services[EOS-25688]
- Handle Motr POD failures[EOS-23426]
- fix rpm build for ARM64 Motr port
- [Containerization] Log rotation policy to be implemented[EOS-25248]
- [Containerization] Implementation of Hare miniprovisioner in k8s environment[EOS-24393]
- Memory leak during Degraded Read operation[EOS-27025] 
- Build motr without kernel dependencies[EOS-26798]
- libfabric-devel is needed for 'make rpms' for motr[EOS-26104]
- Added delays in between ldap operations while create and delete S3 accounts[EOS-26124]
- Upgrade log4j to 2.17.0 
- Implementation for the log rotate config files for K8S[EOS-24355]
- Kubernetes haproxy micro-provisioner code changes[EOS-24655]
- S3 Audit Log: Expose an API to push audit logs[EOS-26021]
- Containerize health monitor service[EOS-25519]
- Containerize health monitor service[EOS-25519]
- Dependent packages upgrade for CSM GUI[EOS-26783]
- Performance Stats: Expose a CSM REST API for exposing performance data with Prometheus support[EOS-26011]
- Audit logs interferring with log rotation[EOS-25913]
- Cortx service Integration: Unable to configure multiple ldap servers[EOS-25405]
- Containerization : Define CSM Agent container entry point[EOS-24102]


## New Features

- Introducing logging for IEM framework[EOS-23255]
- Accommodate new config keys in kubernetes environment[EOS-24357]

