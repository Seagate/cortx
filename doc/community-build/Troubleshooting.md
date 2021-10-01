# Troubleshooting

The troubleshooting document provides information on commonly faced issues while deploying and configuring the CORTX Stack. This document also provides the workaround or resolution to resolve these issues.


1. **S3 endpoint fail to connect on port 80**

   If Installed Operating System's time is not Synchronized then S3 endpoint will fail to connect on port 80.
    
   **Resolution:**
   ```
   yum install ntp ntpdate -y
   systemctl start ntpd
   ntpdate -u -s ntp-b.nist.gov
   systemctl restart ntpd
   timedatectl status
   hwclock -w
   ```
