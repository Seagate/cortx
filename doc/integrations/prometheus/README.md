**What is Prometheus?**
Prometheus is an open-source systems monitoring and alerting toolkit originally built at SoundCloud. Since its inception in 2012, many companies and organizations have adopted Prometheus, and the project has a very active developer and user community. It is now a standalone open source project and maintained independently of any company.

**What is CORTX?**
CORTX is a distributed object storage system designed for great efficiency, massive capacity, and high HDD-utilization. CORTX is 100% Open Source

**How do CORTX and Prometheus work together?**
CORTX S3 metrics grabbed by telegraf agent input plugin, pulled from prometheus from telegraf output plugin, visualized results in grafana dashboard

**Configuring it all to work with CORTX:**

*Step 0: Telegraf agent on cortx + telegraf.conf changes*

*Step 1:  install prometheus and add to prometheus.yml a new target to pull from (remote cortx machine)*

*Step 2:  install grafana and add prometheus data source pulling from (remote cortx machine)*

this was built during cortx hackathon, demo video and deck uploaded 
