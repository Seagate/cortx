# Troubleshooting

The troubleshooting document provides information on commonly faced issues while deploying and configuring the CORTX Stack. This document also provides the workaround or resolution to resolve these issues.


1. **S3 fails to connect S3 endpoint on port 443 and return error "The difference between request time and current time is too large"**

   If Installed Operating System's time is not Synchronized then connection to S3 endpoint will fail on port 443.
    
   **Resolution:**
   ```
   yum install ntp ntpdate -y
   systemctl start ntpd
   ntpdate -u -s ntp-b.nist.gov
   systemctl restart ntpd
   timedatectl status
   hwclock -w
   ```

2. **S3 client i.e. CyberDuck not able to connect if ntp is not in sync on VM**

   **Resolution:**
   Change ntp server in `/etc/chrony.conf` and then restarted chronyd service by running,
   ```
   systemctl restart chronyd
   chronyc makestep
   ```
   
3. **When building the CORTX packages an error message will be returned stating missing `kernel-devel` package**

   ```sh
   Error: No Package found for kernel-devel = 3.10.0-1127.19.1.el7
   error: Failed build dependencies:
   kernel-devel = 3.10.0-1127.19.1.el7 is needed by cortx-motr-2.0.0-0_git2ca587c_3.10.0_1127.19.1.el7.x86_64
   ```

   **Resolution:**
 - Go inside the `Docker` container using the interactive mode by running:
   ```sh
   docker container run -it --rm -v /var/artifacts:/var/artifacts -v /root/cortx:/cortx-workspace ghcr.io/seagate/cortx-build:centos-7.8.2003 bash
   ```
 - Check whether you have `kernel-devel` installed by running `rpm -qa | grep kernel-devel`. If you don't have it, please download the required kernel-devel RPM and then install it.
 - If the `kernel-devel` is installed and you still get the error above, the root cause must be due to the version mismatch. Here is the thing, the `Makefile` script inside the `Docker` calls `uname -r` to get the kernel version. For example, your `uname -r` returns `3.10.0-1127.19.1.el7.x86_64` then the `Makefile` script assumes that the `kernel-devel` RPM must have `3.10.0-1127.19.1` on its name. However, the `kernel-devel` version might differ a bit; instead of `kernel-devel-3.10.0-1127.19.1.el7.x86_64`, it is `kernel-devel-3.10.0-1127.el7.x86_64`.
 - Let's edit the `uname` in the `Docker` to print the correct version as our current `kernel-devel` version. So, make sure you're still inside the `Docker` container (see Step#1).
 - Run these:
   ```sh
   mv /bin/uname /bin/uname.ori
   vi /bin/uname
   ```
 - Then, put this script inside the `/bin/uname`:
   ```sh
   if [ "$1" == "-r" ]; then
      echo "3.10.0-1127.el7.x86_64"
   else
      echo "$(uname.ori $1)"
   fi
   ```
 - Finally, make it executable by running: `chmod +x /bin/uname`
