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
