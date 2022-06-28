# CORTX-Fluentd Integration

![alt text](https://github.com/fluent/fluentd-docs-gitbook/blob/53020426cdcfcb5a5f722031838ee1cb95b5a7a2/images/logo/Fluentd_horizontal.png)
![alt text](https://github.com/Seagate/cortx/blob/main/doc/images/cortx-logo.png)

## Background

**What is Fluentd?**<br/>
Fluentd is an open-source data collection tool for one-stop, unified logging. Fluentd allows you to collect and transform data from various sources for better usage and understanding.

**What is CORTX?**<br/>
CORTX is a open-source distributed object storage system designed for great efficiency, massive capacity, responsiveness, and high HDD-utilization.

**Why this integration important?**<br/>
The world is rapidly increasing on not only its data usage, but also the actual amount of data being created. As a result, we need to be able to store this data in a cohesive, organized manner instead of having a jumbled mess. This data can arrive from a variety of sources such as application logs, network logs, data streams, and even from sensors used in IoT devices.
Integrating Fluentd with CORTX allows for organized data collection from all of these domains in a unified layer.

## Concept Video
A video demonstrating the set up and functionality of the integration can be found [here](https://www.loom.com/share/833a9eb79b594354a903d86dcbd4aecb).

## Project Repository
The configuration file used in this integration is located in [this](https://github.com/sahmed007/cortx-fluentd-config) repository.

## Walkthrough

### Step 0 - Prerequisites
The following steps assume you have a CORTX instance already setup and ready to go. To setup an instance, follow the CORTX Quick Start Guide located [here](https://github.com/Seagate/cortx/blob/main/QUICK_START.md). You will need the S3 credentials for your CORTX instance (AWS Access Key, AWS Secret Key, and the S3 Endpoint).

After you have done the above, create a bucket in your CORTX instance for Fluentd to send log data to. For the purposes of this integration, this bucket name will be called ```fluent```, but you can use any bucket name of your choice.

### Step 1 - Environment Setup
Fluentd has a few recommendations on environment setup. The three things recommended are to set up an NTP daemon, increase the number of file descriptors, and optimize the network kernel parameters.
<br/><br/>In a production-environment, setting up an NTP daemon allows for accurate timestamping. 
For a development environment it is not completely necessary. If you choose to set it up, the Fluentd documentation suggests using an [AWS-hosted NTP server](https://docs.aws.amazon.com/AWSEC2/latest/UserGuide/set-time.html).
<br/><br/>Before increasing the number of file descriptors, first check if it is necessary. You can do so by running

```BASH
$ ulimit -n
```

If the output is ```1024```, it is recommended to increase this value. You can do so by changing your ```limits.conf``` file. Open the file by using:
```BASH
$ nano /etc/security/limits.conf
```
Then add the following lines to the file:
```BASH
root soft nofile 65536
root hard nofile 65536
* soft nofile 65536
* hard nofile 65536
```
Finally, environments with multiple Fluentd instances, you need to optimize network kernel parameters. To do so, run ```nano /etc/sysctl.conf``` and edit the file:
```BASH
net.core.somaxconn = 1024
net.core.netdev_max_backlog = 5000
net.core.rmem_max = 16777216
net.core.wmem_max = 16777216
net.ipv4.tcp_wmem = 4096 12582912 16777216
net.ipv4.tcp_rmem = 4096 12582912 16777216
net.ipv4.tcp_max_syn_backlog = 8096
net.ipv4.tcp_slow_start_after_idle = 0
net.ipv4.tcp_tw_reuse = 1
net.ipv4.ip_local_port_range = 10240 65535
```

### Step 2 - Installation
Fluentd can be installed in many ways. You can view all the installation methods [here](https://docs.fluentd.org/installation). For the purpose of this integration example, let us install via a Ruby Gem. If you are using WSL on Windows, you need to install the ```build-essential``` package as well.

To do so, first install ```ruby``` and ```ruby-dev```:
```BASH
$ sudo apt install ruby && sudo apt install ruby-dev
```

Ensure the Ruby version you have downloaded is ```>=2.4``` by running ```ruby --version```.

Now, that we have resolved our dependencies, go ahead and install the Fluentd Ruby Gem and the Fluentd S3 plugin:
```BASH
$ sudo gem install fluentd --no-doc && sudo gem install fluent-plugin-s3 
```

Verify the installation via:
```BASH
$ fluentd --setup ./fluent
```

### Step 3 - Writing Your Fluentd Configuration File
Now that we have Fluentd successfully installed and a CORTX instance successfully running. We are ready to modify the Fluentd configuration. To do so, navigate to the ```fluent``` directory and locate the ```fluent.conf``` file. Open this file with the editor of your choice and replace the contents of the file with the following:

```XML
<source>
  @type http
  port 8888
</source>

<source>
  @type forward
</source>

<match>
  @type s3
   aws_key_id AKIAtEpiGWUcQIelPRlD1Pi6xQ
   aws_sec_key YNV6xS8lXnCTGSy1x2vGkmGnmdJbZSapNXaSaRhK
   s3_bucket fluent
   s3_endpoint http://uvo1hnou2gvc6n3hmpu.vm.cld.sr
   path logs/
   force_path_style true
   buffer_path ~/fluent-logs/s3
   time_slice_format %Y%m%d%H%M
   time_slice_wait 30s
   utc
   buffer_chunk_limit 256m
</match>
```

In this configuration, we have a total of two sources as inputs. The first being an `http` source and the other being a ```forward``` source. The `http` source allows you to send events via HTTP at the designated port. In this case, we can POST a record by hitting ```localhost``` at port ```8888``` using ```curl```. <br/><br/>The ```forward``` input source listens to a TCP socket for events. You can read more about these two input sources and others in the official Fluentd documentation [here](https://docs.fluentd.org/input).

The other part of this configuration lies between the ```match``` tags. This is where we use the S3 plugin as means to have Fluentd write records into our CORTX instance. Replace the `aws_key_id`, `aws_sec_key`, `s3_bucket`, and `s3_endpoint` with your CORTX credentials. <br/><br/> You also need to replace the `buffer_path` value with the location of your choice on the machine where Fluentd will be running. To use the same `buffer_path` value in the configuration above, simply run `mkdir ~/fluent-logs/s3`. The `path` value will be the folder location you decide to store your records in within the `fluent` bucket inside of CORTX. You can find more information and additional configuration about the S3 plugin [here](https://docs.fluentd.org/output/s3).

### Step 4 - Starting the Fluentd Daemon
Now that we have our configuration set, we are ready to launch Fluentd as a daemon. To do so, run the following:
```BASH
fluentd -c ./fluent/fluent.conf
```
The `-c` flag signifies that you are providing the location to the configuration file that was created above. If you created the file elsewhere, change the path accordingly.

### Step 5 - Uploading and Verifying Records
Launch another terminal window while the Fluentd daemon is running in the initial one. Now, test the configuration by a `POST` request via HTTP at the port indicated above:

```BASH
curl -X POST -d 'json={"test": "fluentd-cortx"}' http://localhost:8888
```

Upon the completion of the request, you should be able to see the record in the `fluentd` bucket under `logs`. You can use a GUI-based tool such as [Cyberduck](https://cyberduck.io/). If you prefer to use the command line, an S3 compatible tool should do the trick. You can download the official AWS CLI [here](https://aws.amazon.com/cli/). <br/><br/>After running ```aws configure```, run the following command to see the records stored in your CORTX bucket:

```BASH
aws --endpoint-url http://uvo1hnou2gvc6n3hmpu.vm.cld.sr s3 ls s3://fluent --recursive
```
Specify the ```endpoint-url``` for your CORTX bucket and replace ```s3://fluent``` with ```s3://<YOUR-BUCKET-NAME```.

You will see an output of your records as such:
```BASH
2021-04-27 20:59:34        298 logs/202104280158_0.gz
2021-04-27 21:00:35         67 logs/202104280159_0.gz
```

Congrats! If you have made it this far, you have successfully integrated CORTX with Fluentd! 

## What's next?
Typically, Fluentd is used in conjuction with Elasticsearch and Kibana (EFK stack) which allows for log aggregation, gathering, and visualization. It would be great to eventually store logs in CORTX as a form of retention management.

In addition, we are also living in the new age of IoT. Devices are gathering information from their environment via sensors. It would be great to extend this integration to send IoT logging data via Fluentd and store it for further analysis and data insight within CORTX.

## Contributors
- **Samad Ahmed**
