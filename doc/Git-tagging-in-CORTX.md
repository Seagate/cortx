The tagging feature has been implemented in the CORTX repos. The tagging feature allows you to check out from a specific version or build of CORTX. You can also use tags to clone and checkout from individual CORTX components repos.

The following repos have the CORTX tagging feature:

- cortx
- cortx-motr
- cortx-s3server
- cortx-manager
- cortx-monitor
- cortx-management-portal
- cortx-ha
- cortx-hare
- cortx-prvsnr
- cortx-utils

To clone any individual CORTX component repo, use:

```
  git clone <component repo>
  cd <component repo dir>
  git checkout <tag>

```

For example:

```
  git clone https://github.com/Seagate/cortx-ha.git
  cd cortx-ha
  git checkout CORTX-OVA-2.0.0-264

```

To clone entire CORTX Stack:

```
  cd /root && git clone https://github.com/Seagate/cortx --recursive --depth=1
  docker run --rm -v /root/cortx:/cortx-workspace ghcr.io/seagate/cortx-build:centos-7.8.2003 make checkout BRANCH=CORTX-OVA-2.0.0-264
  cd <component repo dir>

```
