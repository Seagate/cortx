# Troubleshooting

The troubleshooting document provides information on commonly faced issues while deploying and configuring the CORTX Stack. This document also provides the workaround or resolution to resolve these issues.


1. **CORTX Stack deployment fails if the firewall is not running**

    While deploying the CORTX stack, if due to any reason the firewall is not running then the CORTX deployment fails. 

    **Resolution:** Restart the firewall using following command then rerun the failed commands:
    ```
    systemctl start firewalld
    systemctl enable firewalld
    ```
